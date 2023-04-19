#include <vector>
#include <string>

#include <Arduino.h>
#include <LittleFS.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>
#include <PicoMQTT.h>
#include <PicoPrometheus.h>

#include <utils/io.h>
#include <utils/json_config.h>
#include <utils/periodic_run.h>
#include <utils/reset_button.h>
#include <utils/shift_register.h>
#include <utils/wifi_control.h>
#include <utils/rest.h>

#include "valve.h"

PicoMQTT::Client & get_mqtt() {
    static PicoMQTT::Client mqtt("calor.local", 1883, "valvola");
    return mqtt;
}

PicoMQTT::Publisher & get_mqtt_publisher() {
    return get_mqtt();
}

Prometheus & get_prometheus() {
    static Prometheus prometheus;
    return prometheus;
}

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

std::vector<Valve> valves = {
    Valve(relay_1, "valve 1", 5 * 60 * 1000),
    Valve(relay_2, "valve 2", 5 * 60 * 1000),
    Valve(relay_3, "valve 3", 5 * 60 * 1000),
    Valve(relay_4, "valve 4", 5 * 60 * 1000),
};

PinInput<D1, true> button;
ResetButton reset_button(button);

PinOutput<D4, true> wifi_led;
WiFiControl wifi_control(wifi_led);

RestfulWebServer server(80);

const char CONFIG_FILE[] PROGMEM = "/config.json";

PeriodicRun mqtt_publish_proc(30, 15, [] {
    for (const auto & valve : valves) {
        valve.update_mqtt();
    }
});

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

        json["mqtt"] = get_mqtt().connected();

        server.sendJson(json);
    });

    server.on("/config", HTTP_GET, [] { server.sendJson(get_config()); });

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
                    server.sendJson(valve.get_config());
                    return;
                }

            default:
                server.send(405);
                return;
        }
    });

    get_prometheus().labels["module"] = "valvola";
    get_prometheus().register_metrics_endpoint(server);

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

    get_mqtt().subscribe("valvola/valve/+/request", [](const char * topic, const char * payload) {
        const std::string name = PicoMQTT::Client::get_topic_element(topic, 2).c_str();
        const bool demand_open = (strcmp(payload, "open") == 0);
        Serial.printf("Received request to %s valve %s.\n", demand_open ? "open" : "close", name.c_str());
        for (auto & valve : valves) {
            if (valve.get_name() == name) {
                valve.demand_open = demand_open;
                break;
            }
        }
    });
}

void loop() {
    wifi_control.tick();
    server.handleClient();
    for (auto & valve : valves) {
        valve.tick();
    }
    get_mqtt().loop();
    mqtt_publish_proc.tick();
}
