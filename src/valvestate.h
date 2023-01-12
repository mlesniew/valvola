#ifndef VALVESTATE_H
#define VALVESTATE_H

#include <string>

enum class ValveState {
    closed = 0,
    opening = 1,
    closing = 2,
    open = 3,
    error = -1,
};

const char * to_c_str(const ValveState & s);

#endif
