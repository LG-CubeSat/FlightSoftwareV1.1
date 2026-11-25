/* Real OBC commands + EPS + data, without an interactive sender. */
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "obc_ipc.h"
#include "obc_relay_protocol.h"
#include "obc_telemetry_protocol.h"
#include "eps_command_client.h"
#include "radio.h"

static pid_t children[3] = {-1, -1, -1};
static int failures;
static char directory[] = "/tmp/eps-communications-XXXXXX";
static char storage_path[256];
static char log_path[256];

static void cleanup(void)
{
    for (int i = 2; i >= 0; --i) {
        if (children[i] > 0) {
            kill(children[i], SIGTERM);
            waitpid(children[i], NULL, 0);
            children[i] = -1;
        }
    }
}

static void timeout_handler(int signal_number)
{
    (void)signal_number;
    for (int i = 0; i < 3; ++i) {
        if (children[i] > 0) kill(children[i], SIGKILL);
    }
    _exit(124);
}

static void check(const char *name, int passed)
{
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", name);
    failures += !passed;
}

static int active_socket(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 1;
    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, sizeof(address.sun_path), "%s", path);
    int active = connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0;
    close(fd);
    return active;
}

static pid_t start(const char *executable, const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.log", directory, name);
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (fd < 0) return -1;
    pid_t child = fork();
    if (child == 0) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
        execl(executable, executable, (char *)NULL);
        _exit(127);
    }
    close(fd);
    return child;
}

static int wait_for_listener(void)
{
    char path[256], buffer[8192];
    snprintf(path, sizeof(path), "%s/eps.log", directory);
    for (int attempt = 0; attempt < 200; ++attempt) {
        FILE *file = fopen(path, "r");
        if (file != NULL) {
            size_t count = fread(buffer, 1, sizeof(buffer) - 1, file);
            buffer[count] = '\0';
            fclose(file);
            if (strstr(buffer, "Listening on port 11") != NULL) return 1;
        }
        usleep(20000);
    }
    return 0;
}

static int request(uint8_t id, uint32_t sequence, uint16_t length)
{
    relay_request_t request = {
        .dest_addr = EPS_ADDRESS,
        .dest_port = EPS_CMD_PORT,
        .length = length
    };
    command_envelope_t command = {0};
    command.command_id = id;
    command.seq = sequence;
    memcpy(request.payload, &command, sizeof(command));
    if (IPC_send(ROLE_COMMANDS, (const uint8_t *)&request, sizeof(request)) < 0) {
        return -99;
    }
    OBC_Roles_t source = ROLE_COUNT;
    uint8_t buffer[OBC_IPC_MAX_PAYLOAD];
    int length_received = IPC_receive_timeout(&source, buffer, sizeof(buffer), 2500);
    if (source != ROLE_COMMANDS || length_received != sizeof(eps_relay_reply_t)) {
        return -99;
    }
    eps_relay_reply_t reply;
    memcpy(&reply, buffer, sizeof(reply));
    if (reply.magic != EPS_RELAY_REPLY_MAGIC) return -99;
    if (length == sizeof(command) &&
        (reply.command_id != id || reply.sequence != sequence)) return -99;
    return reply.result;
}

static int stored_records(eps_telemetry_t *latest)
{
    FILE *file = fopen(storage_path, "rb");
    if (file == NULL) return 0;
    uint8_t encoded[OBC_TELEMETRY_HEADER_SIZE + sizeof(eps_telemetry_t)];
    int count = 0;
    while (fread(encoded, 1, sizeof(encoded), file) == sizeof(encoded)) {
        obc_telemetry_record_t record;
        if (obc_telemetry_decode(encoded, sizeof(encoded), &record) != OBC_TELEMETRY_OK ||
            record.source_node != EPS_ADDRESS || record.source_port != EPS_TELEM_PORT ||
            record.payload_length != sizeof(*latest)) {
            fclose(file);
            return -1;
        }
        memcpy(latest, record.payload, sizeof(*latest));
        ++count;
    }
    fclose(file);
    return count;
}

static int radio_request(void)
{
    uint8_t payload[2 + sizeof(command_envelope_t)] = {EPS_ADDRESS, EPS_CMD_PORT};
    command_envelope_t command = {0};
    command.command_id = EPS_WIRE_COMMAND_REQUEST_TELEMETRY;
    command.seq = 200;
    memcpy(payload + 2, &command, sizeof(command));
    if (radio_send(payload, sizeof(payload)) != 0) return 0;
    for (int i = 0; i < 150; ++i) {
        char buffer[16384];
        FILE *file = fopen(log_path, "r");
        if (file != NULL) {
            size_t count = fread(buffer, 1, sizeof(buffer) - 1, file);
            fclose(file);
            buffer[count] = '\0';
            if (strstr(buffer, "[OBC RADIO RX] EPS command: ACK") != NULL) return 1;
        }
        usleep(20000);
    }
    return 0;
}

int main(void)
{
    /* Do not replace sockets belonging to a running user simulation. */
    const char *paths[] = {"/tmp/comms_i2c.sock", "/tmp/obc_ipc_commands.sock",
                          "/tmp/obc_ipc_data.sock", "/tmp/obc_ipc_mission.sock"};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        if (active_socket(paths[i])) {
            fprintf(stderr, "Stop running simulations before this test: %s\n", paths[i]);
            return 77;
        }
    }
    if (mkdtemp(directory) == NULL) return 1;
    printf("Test logs: %s\n", directory);
    snprintf(storage_path, sizeof(storage_path), "%s/telemetry.bin", directory);
    setenv("OBC_TELEMETRY_LOG_PATH", storage_path, 1);
    snprintf(log_path, sizeof(log_path), "%s/obc.jsonl", directory);
    setenv("OBC_LOG_PATH", log_path, 1);
    /* Pick an ephemeral UDP port rather than using the user's ground port. */
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t address_length = sizeof(address);
    if (udp < 0 || bind(udp, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        getsockname(udp, (struct sockaddr *)&address, &address_length) != 0) return 1;
    char port[16];
    snprintf(port, sizeof(port), "%u", (unsigned)ntohs(address.sin_port));
    close(udp);
    setenv("RADIO_SIM_RX_PORT", port, 1);
    setenv("RADIO_SIM_TX_PORT", port, 1);
    signal(SIGALRM, timeout_handler);
    alarm(25);
    atexit(cleanup);

    children[0] = start(OBC_DATA_PATH, "data");
    children[1] = start(OBC_COMMANDS_PATH, "commands");
    children[2] = start(EPS_SIM_PATH, "eps");
    if (children[0] <= 0 || children[1] <= 0 || children[2] <= 0) return 1;
    check("EPS listener started without Enter", wait_for_listener());
    if (failures || IPC_initialize(ROLE_MISSION) != IPC_OK) return 1;

    eps_telemetry_t latest = {0};
    int before = 0;
    for (int i = 0; i < 150 && before < 2; ++i) {
        usleep(20000);
        before = stored_records(&latest);
    }
    check("periodic EPS reports persisted by real OBC data", before >= 2);
    check("sensor timestamp, sequence, and validity carried", latest.sensor_sequence > 0 &&
          latest.sensor_timestamp_ms > 0 && latest.valid_sensor_mask != 0);
    check("invalid estimate still reported as NaN", latest.state_of_flow == 1 &&
          isnan(latest.estimated_time_to_empty_h));

    check("OBC relay receives correlated ACK", request(90, 100, sizeof(command_envelope_t)) ==
          EPS_COMMAND_ACCEPTED);
    int after = before;
    for (int i = 0; i < 40 && after <= before; ++i) {
        usleep(10000);
        after = stored_records(&latest);
    }
    check("requested telemetry persisted promptly", after > before);
    check("duplicate sequence re-ACKed", request(90, 100, sizeof(command_envelope_t)) ==
          EPS_COMMAND_ACCEPTED);
    check("unknown ID NACKed by EPS", request(255, 101, sizeof(command_envelope_t)) ==
          EPS_COMMAND_REJECTED);
    check("short request rejected safely", request(90, 102, 1) == EPS_COMMAND_INVALID_REQUEST);
    check("oversized relay length rejected safely", request(90, 103, 129) ==
          EPS_COMMAND_INVALID_REQUEST);
    check("sequence wrap accepted", request(90, UINT32_MAX, sizeof(command_envelope_t)) ==
          EPS_COMMAND_ACCEPTED && request(90, 0, sizeof(command_envelope_t)) == EPS_COMMAND_ACCEPTED);
    check("radio uplink path receives EPS acknowledgment", radio_request());

    /* No ACK from a stopped EPS: proves the production relay wait is bounded. */
    kill(children[2], SIGSTOP);
    check("unresponsive EPS times out", request(90, 104, sizeof(command_envelope_t)) ==
          EPS_COMMAND_TIMEOUT);
    kill(children[2], SIGCONT);
    cleanup();
    alarm(0);
    return failures ? 1 : 0;
}
