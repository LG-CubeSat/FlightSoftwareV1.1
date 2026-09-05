#include "ingest.h"

#include <stdio.h>
#include <csp/csp.h>
#include "pthread.h"
#include <string.h>
#include "csp_commands.h"

typedef struct {
    uint8_t port;
    OBC_Roles_t owner;
    const char *name;
} ingest_route_t;

static const ingest_route_t routes[] = {
    { ADCS_TELEM_PORT, ROLE_MISSION, "adcs telemetry" },
    { EPS_TELEM_PORT, ROLE_MISSION, "eps telemetry" },
    { ADCS_STATUS_PORT, ROLE_FDIR, "adcs reset notice" },
    // new board comes online -> add one line here, nothing else changes
};

static const ingest_route_t *find_route(uint8_t port)
{
    for (size_t i = 0; i < sizeof(routes)/sizeof(routes[0]); i++) {
        if (routes[i].port == port) return &routes[i];
    }
    return NULL;
}



int ingest_thread_init()
{
    printf("[INGEST THREAD] Attempting Init.\n");
    pthread_t ingest_pthread;
    int ret = pthread_create(&ingest_pthread, NULL, ingest_thread, NULL);
    
    if (ret != 0) {
        fprintf(stderr, "[INGEST THREAD] Thread failed to create: %d\n", ret);
    } else {
        printf("[INGEST THREAD] Init Successful.\n");
    }
    return ret;
}

void *ingest_thread(void *arg)
{
    (void)arg;
    
    csp_socket_t sock = {0};
    // TODO: expand to each, not just ADCS
    csp_bind(&sock, CSP_ANY);
    csp_listen(&sock, 5);

    for (;;) {
        csp_conn_t *conn = csp_accept(&sock, 10000);
        if (conn == NULL) continue;

        uint8_t dport = csp_conn_dport(conn);
        const ingest_route_t *route = find_route(dport);

        csp_packet_t *packet;
        while ((packet = csp_read(conn, 50)) != NULL) {
            if (route != NULL) {
                printf("[INGEST] %s: %d bytes -> role %d\n", route->name, packet->length, route->owner);
                fflush(stdout);
                if (IPC_send(route->owner, packet->data, packet->length) < 0) {
                    fprintf(stderr, "[INGEST] forward to role %d failed (not listening?)\n", route->owner);
                }
            } else {
                fprintf(stderr, "[INGEST] no route for port %d, dropping\n", dport);
            }
            csp_buffer_free(packet);
        }
        csp_close(conn);
    }

    return NULL;
}