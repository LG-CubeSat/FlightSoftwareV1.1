#include "relay.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <csp/csp.h>
#include "pthread.h"
#include "obc_ipc.h"
#include "obc_relay_protocol.h"
#include "commands_health.h"

#define RELAY_POLL_TIMEOUT_MS 1000

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
        int len = IPC_receive_timeout(
            &src,
            buf,
            sizeof(buf),
            RELAY_POLL_TIMEOUT_MS
        );
        if (len == IPC_TIMEOUT) {
            commands_health_relay_progress();
            continue;
        }
        if (len < 0) {
            continue;
        }
        if (len != sizeof(relay_request_t)) {
            commands_health_relay_progress();
            continue;
        }
        
        relay_request_t req;
        memcpy(&req, buf, sizeof(req));

        printf("[RELAY] DEBUG: got request from role %d -> addr=%d port=%d length=%d\n",
               src, req.dest_addr, req.dest_port, req.length);
        fflush(stdout);

        // prio norm indicates medium priority level
        csp_conn_t *conn = csp_connect(CSP_PRIO_NORM, req.dest_addr, req.dest_port, 1000, CSP_O_NONE);
        if (conn == NULL) {
            fprintf(stderr, "[RELAY] Connect to addr=%d port=%d failed\n", req.dest_addr, req.dest_port);
            commands_health_relay_progress();
            continue;
        }
        printf("[RELAY] DEBUG: connected, sending now\n");
        fflush(stdout);

        csp_packet_t *packet = csp_buffer_get(0);
        if (packet == NULL) {
            fprintf(stderr, "[RELAY] out of packet buffers\n");
            csp_close(conn);
            commands_health_relay_progress();
            continue;
        }

        memcpy(packet->data, req.payload, req.length);
        packet->length = req.length;
        csp_send(conn, packet);
        csp_close(conn);
        commands_health_relay_progress();
    }
    return NULL;
}
