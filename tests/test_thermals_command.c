#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csp/csp.h>

#include "csp_commands.h"
#include "csp_network.h"

#define TEST_TIMEOUT_SECONDS (20)

int main(void)
{
    pid_t thermals_pid;
    csp_conn_t *connection;
    csp_packet_t *packet;
    csp_packet_t *reply;
    thermal_command_t command;
    command_ack_t acknowledgement;
    int test_passed = 0;

    alarm(TEST_TIMEOUT_SECONDS);

    unlink("/tmp/comms_i2c.sock");

    csp_network_init(OBC_ADDRESS, 1);

    thermals_pid = fork();

    if (thermals_pid < 0)
    {
        printf("[THERMALS COMMAND TEST] Failed to start Thermals.\n");
        return 1;
    }

    if (thermals_pid == 0)
    {
        execl(
            THERMALS_SIM_PATH,
            THERMALS_SIM_PATH,
            (char *)NULL
        );

        _exit(127);
    }

    usleep(500000);

    connection = csp_connect(
        CSP_PRIO_NORM,
        THERMALS_ADDRESS,
        THERMALS_CMD_PORT,
        1000,
        CSP_O_NONE
    );

    if (connection == NULL)
    {
        printf("[THERMALS COMMAND TEST] Connection failed.\n");
        goto cleanup;
    }

    packet = csp_buffer_get(0);

    if (packet == NULL)
    {
        printf("[THERMALS COMMAND TEST] Packet allocation failed.\n");
        csp_close(connection);
        goto cleanup;
    }

    command.envelope.command_id =
        THERMALS_WIRE_COMMAND_SET_TARGET_TEMP;

    command.envelope.seq = 1u;
    command.target_temp = 25.0f;

    memcpy(packet->data, &command, sizeof(command));
    packet->length = sizeof(command);

    if (!csp_send(connection, packet))
    {
        printf("[THERMALS COMMAND TEST] Command send failed.\n");
        csp_close(connection);
        goto cleanup;
    }

    reply = csp_read(connection, 1000);

    if (reply == NULL)
    {
        printf("[THERMALS COMMAND TEST] No ACK received.\n");
        csp_close(connection);
        goto cleanup;
    }

    if (reply->length < sizeof(acknowledgement))
    {
        printf("[THERMALS COMMAND TEST] ACK packet was too small.\n");
    }
    else
    {
        memcpy(
            &acknowledgement,
            reply->data,
            sizeof(acknowledgement)
        );

        if (acknowledgement.ack_command_id ==
                THERMALS_WIRE_COMMAND_SET_TARGET_TEMP &&
            acknowledgement.ack_seq == 1u &&
            acknowledgement.status == ACK)
        {
            printf(
                "[THERMALS COMMAND TEST] PASS: target command was ACKed.\n"
            );

            test_passed = 1;
        }
        else
        {
            printf(
                "[THERMALS COMMAND TEST] FAIL: incorrect ACK response.\n"
            );
        }
    }

    csp_buffer_free(reply);
    csp_close(connection);

cleanup:
    kill(thermals_pid, SIGTERM);
    waitpid(thermals_pid, NULL, 0);

    return test_passed ? 0 : 1;
}