#include "metrics.h"

namespace metrics {

Prometheus prometheus;

PrometheusGauge valve_state(prometheus, "valve_state", "Valve state enum");

}
