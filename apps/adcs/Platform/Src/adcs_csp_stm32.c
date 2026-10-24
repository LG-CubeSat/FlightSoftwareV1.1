#include "adcs_board.h"
#include "adcs_i2c_slave.h"
#include "adcs_log.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "frame.h"
#include "csp_commands.h"
#include <csp/csp.h>
#include <csp/csp_id.h>
#include <string.h>

#define TX_QUEUE_LENGTH 2
static csp_iface_t interface;
static QueueHandle_t tx_queue;
static StaticQueue_t tx_queue_control;
static uint8_t tx_queue_storage[TX_QUEUE_LENGTH * sizeof(adcs_i2c_frame_t)];
static StaticTask_t bus_tcb, router_tcb;
static StackType_t bus_stack[384], router_stack[384];

static int send_packet(csp_iface_t *iface, uint16_t via, csp_packet_t *packet, int from_me) {
    (void)via;
    (void)from_me;
    Frame frame = {.dest_addr = (uint8_t)packet->id.dst, .src_addr = ADCS_ADDRESS};
    adcs_i2c_frame_t wire;
    csp_id_prepend(packet);
    int result = CSP_ERR_TX;
    if (packet->frame_length <= MAX_FRAME_PAYLOAD) {
        frame.length = packet->frame_length;
        memcpy(frame.payload, packet->frame_begin, frame.length);
        int length = frame_serialize(&frame, wire.bytes, sizeof(wire.bytes));
        if (length > 0) {
            wire.length = (uint16_t)length;
            if (xQueueSend(tx_queue, &wire, 0) == pdPASS) result = CSP_ERR_NONE;
        }
    }
    csp_buffer_free(packet); /* This driver owns the buffer on success and failure. */
    if (result != CSP_ERR_NONE) iface->tx_error++;
    return result;
}
static void receive_frame(const adcs_i2c_frame_t *wire) {
    Frame frame;
    if (frame_deserialize(wire->bytes, wire->length, &frame) < 0 ||
        frame.dest_addr != ADCS_ADDRESS) {
        interface.rx_error++;
        return;
    }
    csp_packet_t *packet = csp_buffer_get(0);
    if (packet == NULL) { interface.rx_error++; return; }
    int header_size = csp_id_setup_rx(packet);
    if (frame.length < header_size || frame.length > sizeof(packet->data) + header_size) {
        csp_buffer_free(packet);
        interface.rx_error++;
        return;
    }
    memcpy(packet->frame_begin, frame.payload, frame.length);
    packet->frame_length = frame.length;
    if (csp_id_strip(packet) != CSP_ERR_NONE || packet->id.dst != ADCS_ADDRESS ||
        packet->id.src != frame.src_addr) {
        csp_buffer_free(packet);
        interface.rx_error++;
        return;
    }
    csp_qfifo_write(packet, &interface, NULL);
}
static void bus_task(void *arg) {
    (void)arg;
    adcs_i2c_frame_t rx, pending;
    int has_pending = 0;
    TickType_t last_led = xTaskGetTickCount();
    for (;;) {
        adcs_i2c_slave_process();
        if (adcs_i2c_slave_take_received_frame(&rx)) receive_frame(&rx);
        if (!has_pending) has_pending = xQueueReceive(tx_queue, &pending, 0) == pdPASS;
        if (has_pending && adcs_i2c_slave_queue_transmit_frame(pending.bytes, pending.length) == HAL_OK)
            has_pending = 0;
        uint32_t events = adcs_i2c_slave_take_events();
        if (events & (ADCS_I2C_EVENT_ERROR | ADCS_I2C_EVENT_MALFORMED_FRAME))
            interface.rx_error++;
        if ((TickType_t)(xTaskGetTickCount() - last_led) >= pdMS_TO_TICKS(1000)) {
            last_led = xTaskGetTickCount();
            HAL_GPIO_TogglePin(LED_Status_GPIO_Port, LED_Status_Pin);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
static void router_task(void *arg) {
    (void)arg;
    for (;;) csp_route_work();
}
void adcs_csp_init(void) {
    tx_queue = xQueueCreateStatic(TX_QUEUE_LENGTH, sizeof(adcs_i2c_frame_t),
                                 tx_queue_storage, &tx_queue_control);
    configASSERT(tx_queue != NULL);
    csp_init();
    interface.name = "I2C";
    interface.addr = ADCS_ADDRESS;
    interface.is_default = 1;
    interface.nexthop = send_packet;
    csp_iflist_add(&interface);
    configASSERT(xTaskCreateStatic(bus_task, "I2C", 384, NULL, 3, bus_stack, &bus_tcb));
    configASSERT(xTaskCreateStatic(router_task, "CSPRouter", 384, NULL, 3, router_stack, &router_tcb));
}
