/* Deterministic failure-path tests: no processes, sockets, or hardware. */
#include <stdio.h>
#include <string.h>
#include <csp/csp.h>
#include "eps_command_client.h"

static csp_packet_t outgoing, incoming;
static int connection_token;
static int fail_connect, fail_buffer, no_reply;
static int closes, frees, sends, failures;

csp_conn_t *csp_connect(uint8_t prio, uint16_t destination, uint8_t port,
                        uint32_t timeout, uint32_t options)
{
    (void)prio; (void)destination; (void)port; (void)timeout; (void)options;
    return fail_connect ? NULL : (csp_conn_t *)&connection_token;
}
csp_packet_t *csp_buffer_get(size_t unused)
{
    (void)unused;
    return fail_buffer ? NULL : &outgoing;
}
void csp_send(csp_conn_t *connection, csp_packet_t *packet)
{
    (void)connection; (void)packet;
    ++sends;
}
csp_packet_t *csp_read(csp_conn_t *connection, uint32_t timeout)
{
    (void)connection; (void)timeout;
    return no_reply ? NULL : &incoming;
}
void csp_buffer_free(void *packet)
{
    (void)packet;
    ++frees;
}
int csp_close(csp_conn_t *connection)
{
    (void)connection;
    ++closes;
    return 0;
}

static void check(const char *name, int passed)
{
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", name);
    failures += !passed;
}

static void reply_for(const command_envelope_t *command,
                      command_ack_status_t status)
{
    command_ack_t ack = {0};
    ack.ack_command_id = command->command_id;
    ack.ack_seq = command->seq;
    ack.status = status;
    memcpy(incoming.data, &ack, sizeof(ack));
    incoming.length = sizeof(ack);
    fail_connect = fail_buffer = no_reply = 0;
    closes = frees = sends = 0;
}

int main(void)
{
    command_envelope_t command = {0};
    command.command_id = EPS_WIRE_COMMAND_REQUEST_TELEMETRY;
    command.seq = UINT32_MAX;

    check("null rejected", eps_command_send(NULL, sizeof(command)) ==
          EPS_COMMAND_INVALID_REQUEST);
    check("short rejected", eps_command_send(&command, 1) ==
          EPS_COMMAND_INVALID_REQUEST);
    check("oversized rejected", eps_command_send(&command, sizeof(command) + 1) ==
          EPS_COMMAND_INVALID_REQUEST);

    reply_for(&command, ACK);
    check("matching ACK", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_ACCEPTED && closes == 1 && frees == 1 && sends == 1);
    check("payload preserved", outgoing.length == sizeof(command) &&
          memcmp(outgoing.data, &command, sizeof(command)) == 0);

    reply_for(&command, NACK);
    check("matching NACK", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_REJECTED);

    reply_for(&command, ACK);
    --incoming.length;
    check("short ACK rejected", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_INVALID_REPLY && frees == 1 && closes == 1);

    reply_for(&command, ACK);
    command.seq = 0;
    check("wrong sequence rejected", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_INVALID_REPLY);

    reply_for(&command, ACK);
    ++command.command_id;
    check("wrong command ID rejected", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_INVALID_REPLY);

    reply_for(&command, (command_ack_status_t)42);
    check("invalid status rejected", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_INVALID_REPLY);

    reply_for(&command, ACK);
    no_reply = 1;
    check("timeout closes connection", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_TIMEOUT && closes == 1 && frees == 0);

    reply_for(&command, ACK);
    fail_buffer = 1;
    check("allocation failure closes connection", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_NO_BUFFER && closes == 1 && sends == 0);

    reply_for(&command, ACK);
    fail_connect = 1;
    check("connection failure", eps_command_send(&command, sizeof(command)) ==
          EPS_COMMAND_CONNECT_FAILED && closes == 0);
    return failures ? 1 : 0;
}
