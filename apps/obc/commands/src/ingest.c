#include "ingest.h"

#include <stdio.h>
#include <csp/csp.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>

#include "csp_commands.h"
#include "commands_health.h"
#include "obc_telemetry_protocol.h"

#define INGEST_POLL_TIMEOUT_MS 1000

typedef enum {
    INGEST_FORWARD_RAW,
    INGEST_FORWARD_TELEMETRY
} ingest_forward_type_t;


typedef struct {
    uint8_t port;
    OBC_Roles_t owner;
    ingest_forward_type_t forward_type;
    const char *name;
} ingest_route_t;

static const ingest_route_t routes[] = {
    { ADCS_TELEM_PORT, ROLE_MISSION, INGEST_FORWARD_TELEMETRY, "adcs telemetry" },
    { EPS_TELEM_PORT, ROLE_MISSION, INGEST_FORWARD_TELEMETRY, "eps telemetry" },
    { ADCS_STATUS_PORT, ROLE_FDIR, INGEST_FORWARD_RAW, "adcs reset notice" },
    { TIME_SYNC_REQUEST_PORT, ROLE_TIME, INGEST_FORWARD_RAW, "time sync request" }
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

static int forward_telemetry(
    csp_conn_t *conn,
    uint8_t destination_port,
    const csp_packet_t *packet
) {
    if (packet->length > OBC_TELEMETRY_MAX_PAYLOAD) {
        fprintf(
            stderr,
            "[INGEST] telemetry payload too large: %u bytes\n",
            packet->length
        );
        return -1;
    }

    int source_node = csp_conn_src(conn);
    if (source_node < 0 || source_node > UINT8_MAX) {
        fprintf(
            stderr,
            "[INGEST] invalid CSP source node: %d\n",
            source_node
        );
        return -1;
    }

    struct timespec received_at;
    if (clock_gettime(CLOCK_REALTIME, &received_at) != 0) {
        perror("[INGEST] clock_gettime");
        return -1;
    }

    obc_telemetry_record_t record = {
        .source_node = (uint8_t)source_node,
        .source_port = destination_port,
        .received_unix_us =
            (uint64_t)received_at.tv_sec * UINT64_C(1000000) +
            (uint64_t)received_at.tv_nsec / UINT64_C(1000),
        .payload_length = packet->length
    };

    memcpy(
        record.payload,
        packet->data,
        packet->length
    );

    uint8_t encoded[OBC_IPC_MAX_PAYLOAD];
    size_t encoded_size = 0;

    obc_telemetry_status_t status = obc_telemetry_encode(
        &record,
        encoded,
        sizeof(encoded),
        &encoded_size
    );

    if (status != OBC_TELEMETRY_OK) {
        fprintf(stderr,
        "[INGEST] telemetry encoding failed: %d\n", status);
        return -1;
    }

    return IPC_send(
        ROLE_DATA,
        encoded,
        (uint16_t)encoded_size
    );
}

void *ingest_thread(void *arg)
{
    (void)arg;
    
    csp_socket_t sock = {0};
    // TODO: expand to each, not just ADCS
    csp_bind(&sock, CSP_ANY);
    csp_listen(&sock, 5);

    for (;;) {
        csp_conn_t *conn = csp_accept(&sock, INGEST_POLL_TIMEOUT_MS);
        if (conn == NULL) {
            commands_health_ingest_progress();
            continue;
        }

        uint8_t dport = csp_conn_dport(conn);
        const ingest_route_t *route = find_route(dport);

        csp_packet_t *packet;
        while ((packet = csp_read(conn, 50)) != NULL) {
            if (route != NULL) {
                printf("[INGEST] %s: %d bytes -> role %d\n", route->name, packet->length, route->owner);
                fflush(stdout);
                
                int forward_result;

                if (route->forward_type == INGEST_FORWARD_TELEMETRY) {
                    forward_result = forward_telemetry(
                        conn,
                        dport,
                        packet
                    );
                } else {
                    forward_result = IPC_send(
                        route->owner,
                        packet->data,
                        packet->length
                    );
                }

                if (forward_result < 0) {
                    fprintf(
                        stderr,
                        "[INGEST] forward to role %d failed\n",
                        route->owner
                    );
                }

            } else {
                fprintf(stderr, "[INGEST] no route for port %d, dropping\n", dport);
            }
            csp_buffer_free(packet);
            commands_health_ingest_progress();
        }
        csp_close(conn);
        commands_health_ingest_progress();
    }

    return NULL;
}
