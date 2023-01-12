#ifndef METRICS_H
#define METRICS_H

#include <prometheus.h>

namespace metrics {

extern Prometheus prometheus;

extern PrometheusGauge heating_demand;

extern PrometheusGauge valve_state;

}

#endif
