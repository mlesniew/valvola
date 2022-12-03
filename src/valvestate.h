#ifndef VALVESTATE_H
#define VALVESTATE_H

#include <string>


enum class ValveState {
    closed,
    opening,
    open,
    closing,
    error
};

const char * to_c_str(const ValveState & s);

#endif
