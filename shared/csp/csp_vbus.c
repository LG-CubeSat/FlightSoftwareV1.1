/*
Connect libcsp to vbus
transmit csp packets through vbus
recieve csp packets from vbus
*/

#include "../../platform/sim/include/v_bus.h"
#include "../../libs/libcsp/include/csp/csp_types.h"

typedef {
    csp_iface_t iface;
    VBus_t vbus;
} csp_vbus_t;