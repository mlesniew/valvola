#include <vector>
#include <string>

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>
#include <prometheus.h>

#include <utils/io.h>
#include <utils/json_config.h>
#include <utils/reset_button.h>
#include <utils/shift_register.h>
#include <utils/wifi_control.h>

#include "valve.h"

Prometheus prometheus;

class ValvolaValve: public Valve {
    public:
        using Valve::Valve;

        virtual ~ValvolaValve() {
            gauge_valve_state.remove({{"zone", name}});
        }

    protected:
        virtual void on_state_change() const {
            gauge_valve_state[{{"zone", name}}]
                .set(static_cast<typename std::underlying_type<State>::type>((State)state));
        }

    private:
        static PrometheusGauge gauge_valve_state;
};

PrometheusGauge ValvolaValve::gauge_valve_state(prometheus, "valve_state", "Valve state enum");

ShiftRegister<1> shift_register(
    D6,  // data pin
    D5,  // clock pin
    D0,  // latch pin
(uint8_t[1]) {
    0b00001111,
}  // inverted outputs
);

ShiftRegisterOutput relay_1{shift_register, 0};
ShiftRegisterOutput relay_2{shift_register, 1};
ShiftRegisterOutput relay_3{shift_register, 2};
ShiftRegisterOutput relay_4{shift_register, 3};

std::vector<ValvolaValve> valves = {
    ValvolaValve(relay_1, "valve 1", 5 * 60 * 1000),
    ValvolaValve(relay_2, "valve 2", 5 * 60 * 1000),
    ValvolaValve(relay_3, "valve 3", 5 * 60 * 1000),
    ValvolaValve(relay_4, "valve 4", 5 * 60 * 1000),
};

PinInput<D1, true> button;
ResetButton reset_button(button);

PinOutput<D4, true> wifi_led;
WiFiControl wifi_control(wifi_led);

ESP8266WebServer server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

void return_json(const JsonDocument & json, unsigned int code = 200) {
    String output;
    serializeJson(json, output);
    server.send(code, F("application/json"), output);
}

DynamicJsonDocument get_config() {
    DynamicJsonDocument json(1024);

    auto valves_config = json["valves"].to<JsonArray>();
    for (auto & valve : valves) {
        valves_config.add(valve.get_config());
    }

    return json;
}

void setup_server() {
    server.on("/status", HTTP_GET, [] {
        DynamicJsonDocument json(1024);

        auto valve_status = json["valves"].to<JsonArray>();
        for (auto & valve : valves) {
            valve_status.add(valve.get_status());
        }

        return_json(json);
    });

    server.on("/config", HTTP_GET, [] { return_json(get_config()); });

    server.on("/config/save", HTTP_POST, [] {
        const auto json = get_config();
        File f = LittleFS.open(FPSTR(CONFIG_FILE), "w");
        if (!f) {
            server.send(500);
            return;
        }
        serializeJson(json, f);
        f.close();
        server.send(200);
    });

    server.on(UriRegex("/config/valves/([0-9]+)"), [] {
        const unsigned int idx = server.pathArg(0).toInt();

        if (idx >= valves.size()) {
            server.send(404);
            return;
        }

        Valve & valve = valves[idx];

        switch (server.method()) {
            case HTTP_PUT:
            case HTTP_POST:
            case HTTP_PATCH: {
                    StaticJsonDocument<128> json;

                    const auto error = deserializeJson(json, server.arg("plain"));
                    if (error) {
                        server.send(400, "text/plain", error.f_str());
                        return;
                    }

                    if (!valve.set_config(json.as<JsonVariant>())) {
                        server.send(400);
                        return;
                    }
                }
            // fall through

            case HTTP_GET: {
                    return_json(valve.get_config());
                    return;
                }

            default:
                server.send(405);
                return;
        }
    });

    server.on("/valves", [] {
        StaticJsonDocument<512> json;

        switch (server.method()) {
            case HTTP_PUT:
            case HTTP_POST: {
                    const auto error = deserializeJson(json, server.arg("plain"));

                    if (error) {
                        server.send(400, "text/plain", error.f_str());
                        return;
                    }

                    for (const JsonPair kv : json.as<JsonObject>()) {
                        const std::string name = kv.key().c_str();
                        for (auto & valve : valves) {
                            if (valve.name == name) {
                                valve.demand_open = kv.value().as<bool>();
                                valve.tick();
                                break;
                            }
                        }
                    }
                }
            // fall through
            case HTTP_GET: {
                    json.clear();
                    for (auto & valve : valves) {
                        json[valve.name] = to_c_str(valve.get_state());
                    }

                    return_json(json);
                    return;
                }

            default:
                server.send(405);
                return;
        }
    });

    prometheus.labels["module"] = "valvola";
    prometheus.register_metrics_endpoint(server);

    server.begin();
}

void setup() {
    shift_register.init();
    wifi_led.init();
    wifi_led.set(true);

    Serial.begin(115200);
    Serial.println(F("\n\n"
        "             _            _\n"
        " /\\   /\\__ _| |_   _____ | | __ _\n"
        " \\ \\ / / _` | \\ \\ / / _ \\| |/ _` |\n"
        "  \\ V / (_| | |\\ V / (_) | | (_| |\n"
        "   \\_/ \\__,_|_| \\_/ \\___/|_|\\__,_|\n"
        "\n"
        "   Valvola " __DATE__ " " __TIME__ "\n"
        "\n\n"
        "Press and hold button now to enter WiFi setup.\n"
        ));

    delay(3000);
    reset_button.init();
    wifi_control.init(button ? WiFiInitMode::setup : WiFiInitMode::saved, "valvola");

    LittleFS.begin();

    {
        const auto config = JsonConfigFile(LittleFS, FPSTR(CONFIG_FILE), 1024);

        const auto valves_config = config["valves"].as<JsonArrayConst>();
        for (unsigned int idx = 0; idx < valves.size(); ++idx) {
            valves[idx].set_config(valves_config[idx].as<JsonVariantConst>());
        }
    }

    setup_server();
}

void loop() {
    server.handleClient();
    wifi_control.tick();
    for (auto & valve : valves) {
        valve.tick();
    }
}
