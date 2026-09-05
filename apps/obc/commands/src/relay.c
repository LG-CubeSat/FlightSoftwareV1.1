#include "relay.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <csp/csp.h>
#include "pthread.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"

int relay_thread_init()
{
    printf("[RELAY THREAD] Attempting Init.\n");

    pthread_t relay_pthread;
    
    int ret = pthread_create(&relay_pthread, NULL, relay_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "[RELAY THREAD] Failed to initialize: %d\n", ret);
    } else {
        printf("[RELAY THREAD] Successfully initialized.\n");
    }
    return ret;
}

void *relay_thread(void *arg) 
{
    (void)arg;
    
    for (;;) {
        OBC_Roles_t src;
        uint8_t buf[sizeof(relay_request_t)];
        int len = IPC_receive(&src, buf, sizeof(buf));
        if (len != sizeof(relay_request_t)) continue; // malformed, not correct fit
        
        relay_request_t req;
        memcpy(&req, buf, sizeof(req));

        // prio norm indicates medium priority level
        csp_conn_t *conn = csp_connect(CSP_PRIO_NORM, req.dest_addr, req.dest_port, 1000, CSP_O_NONE);
        if (conn == NULL) {
            fprintf(stderr, "[RELAY] Connect to addr=%d port=%d failed\n", req.dest_addr, req.dest_port);
            continue;
        }

        csp_packet_t *packet = csp_buffer_get(0);
        if (packet == NULL) {
            fprintf(stderr, "[RELAY] out of packet buffers\n");
            csp_close(conn);
            continue;
        }

        memcpy(packet->data, req.payload, req.length);
        packet->length = req.length;
        csp_send(conn, packet);
        csp_close(conn);
    }
    return NULL;
}