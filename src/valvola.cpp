#include <vector>
#include <string>

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <uri/UriRegex.h>

#include <ArduinoJson.h>

#include <utils/io.h>
#include <utils/wifi_control.h>

#include "valve.h"

PinOutput<D0, true> relay_1;
PinOutput<D5, true> relay_2;
PinOutput<D6, true> relay_3;
PinOutput<D7, true> relay_4;

std::vector<Valve> valves = {
    Valve(relay_1, "valve 1", 20 * 1000),
    Valve(relay_2, "valve 2", 20 * 1000),
    Valve(relay_3, "valve 3", 20 * 1000),
    Valve(relay_4, "valve 4", 20 * 1000),
};

PinInput<D1, true> button;

PinOutput<D4, true> wifi_led;
WiFiControl wifi_control(wifi_led);

ESP8266WebServer server(80);

void setup_server() {
    server.on(UriRegex("/valves/([0-9]+)/config"), [] {
            const int idx = server.pathArg(0).toInt();

            if (idx >= valves.size()) {
                server.send(404);
                return;
            }

            Valve & valve = valves[idx];

            switch (server.method()) {
            case HTTP_PUT:
            case HTTP_POST:
            case HTTP_PATCH:
            {
                StaticJsonDocument<128> json;

                const auto error = deserializeJson(json, server.arg("plain"));
                if (error) {
                    server.send(400, "text/plain", error.f_str());
                    return;
                }

                if (!valve.load(json.as<JsonVariant>())) {
                    server.send(400);
                    return;
                }
            }
            // fall through

            case HTTP_GET:
            {
                const auto json = valve.to_json();

                String output;
                serializeJson(json, output);
                server.send(200, "application/json", output);

                return;
            }

            default:
                server.send(405);
                return;
            }
    });

    server.on("/valves", [] {
        StaticJsonDocument<128> json;

        switch (server.method()) {
            case HTTP_PUT:
            case HTTP_POST:
            {
                const auto error = deserializeJson(json, server.arg("plain"));

                if (error) {
                    server.send(400, "text/plain", error.f_str());
                    return;
                }

                for (const JsonPair kv: json.as<JsonObject>()) {
                    const std::string name = kv.key().c_str();
                    for (auto & valve: valves) {
                        if (valve.name == name) {
                            valve.demand_open = kv.value().as<bool>();
                            valve.tick();
                            break;
                        }
                    }
                }
            }
            // fall through
            case HTTP_GET:
            {
                json.clear();
                for (auto & valve: valves) {
                    json[valve.name] = to_c_str(valve.get_state());
                }

                String output;
                serializeJson(json, output);
                server.send(200, "application/json", output);

                return;
            }

            default:
                server.send(405);
                return;
        }
    });

    server.begin();
}

void setup() {
    Serial.begin(9600);

    wifi_led.init();

    const auto mode = WiFiInitMode::saved; // button ? WiFiInitMode::setup : WiFiInitMode::saved;
    wifi_control.init(mode, "valvola");

    setup_server();
}

void loop() {
    server.handleClient();
    wifi_control.tick();
    for (auto & valve: valves) {
        valve.tick();
    }
}
