#ifndef VALVE_H
#define VALVE_H

#include <string>

#include <utils/io.h>
#include <utils/tickable.h>

#include <ArduinoJson.h>

#include "timedvalue.h"
#include "valvestate.h"


class Valve: public Tickable {
public:
    Valve(BinaryOutput & output, const std::string & name = "", const unsigned long switch_time_millis = 0);

    void tick() override;
    ValveState get_state() const {
        return state;
    }

    DynamicJsonDocument to_json() const;
    bool load(const JsonVariant & json);

    std::string name;
    unsigned long switch_time_millis;
    bool demand_open;

protected:
    BinaryOutput & output;
    TimedValue<ValveState> state;
};

#endif
