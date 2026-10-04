#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "obc_ipc.h"

#define CHILD_TIMEOUT_SEC 3
#define RECEIVE_TIMEOUT_MS 1000

static const char *const data_socket = "/tmp/obc_ipc_data.sock";
static const char *const compute_socket = "/tmp/obc_ipc_compute.sock";

static void cleanup_sockets(void)
{
    unlink(data_socket);
    unlink(compute_socket);
}

static int run_compute_peer(int ready_fd)
{
    alarm(CHILD_TIMEOUT_SEC);

    if (IPC_initialize(ROLE_COMPUTE) != IPC_OK) return 10;
    if (write(ready_fd, "R", 1) != 1) return 11;
    close(ready_fd);

    OBC_Roles_t src = 0;
    uint8_t request[16] = {0};
    int length = IPC_receive_timeout(&src, request, sizeof(request), RECEIVE_TIMEOUT_MS);
    if (length != 4 || src != ROLE_DATA || memcmp(request, "PING", 4) != 0) return 12;
    if (IPC_send(ROLE_DATA, (const uint8_t *)"PONG", 4) != 4) return 13;

    return 0;
}

int main(void)
{
    cleanup_sockets();

    int ready_pipe[2];
    if (pipe(ready_pipe) != 0) {
        perror("pipe");
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        close(ready_pipe[0]);
        _exit(run_compute_peer(ready_pipe[1]));
    }

    close(ready_pipe[1]);
    int failed = 0;

    if (IPC_initialize(ROLE_DATA) != IPC_OK) {
        fprintf(stderr, "[FAIL] parent initializes the data-role socket\n");
        failed = 1;
    }

    char ready = 0;
    if (!failed && (read(ready_pipe[0], &ready, 1) != 1 || ready != 'R')) {
        fprintf(stderr, "[FAIL] compute peer becomes ready\n");
        failed = 1;
    }
    close(ready_pipe[0]);

    if (!failed && IPC_send(ROLE_COMPUTE, (const uint8_t *)"PING", 4) != 4) {
        fprintf(stderr, "[FAIL] data role sends a framed message to compute\n");
        failed = 1;
    }

    OBC_Roles_t src = 0;
    uint8_t reply[16] = {0};
    int length = failed ? IPC_ERROR
                        : IPC_receive_timeout(&src, reply, sizeof(reply), RECEIVE_TIMEOUT_MS);
    if (!failed && (length != 4 || src != ROLE_COMPUTE || memcmp(reply, "PONG", 4) != 0)) {
        fprintf(stderr, "[FAIL] data role receives the reply with its source identity\n");
        failed = 1;
    }

    if (!failed && IPC_receive_timeout(&src, reply, sizeof(reply), 25) != IPC_TIMEOUT) {
        fprintf(stderr, "[FAIL] an idle receive returns IPC_TIMEOUT\n");
        failed = 1;
    }

    int status = 0;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[FAIL] compute peer completed successfully\n");
        failed = 1;
    }

    cleanup_sockets();

    if (failed) {
        fprintf(stderr, "obc_ipc_test: FAIL\n");
        return 1;
    }

    printf("[PASS] messages retain payload and source role\n");
    printf("[PASS] receive timeout is bounded\n");
    printf("obc_ipc_test: PASS\n");
    return 0;
}
