#ifndef COMMS_I2C_H
#define COMMS_I2C_H

#include "comms_bus.h"

CommsBus_t create_comms_bus(void);

static void * loop_accept_new_connections(void * param);

CommsBusStatus_t comms_bus_initialize(uint8_t my_address, int is_master);

int comms_check_new_connections(int bus_fd);

int comms_bus_send(uint8_t dest_addr, const uint8_t *data, uint8_t length);

int comms_bus_receive(uint8_t *src_addr_out, uint8_t *buffer, uint16_t max_length);

#endif