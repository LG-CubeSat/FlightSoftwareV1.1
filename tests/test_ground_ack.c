#include "ground_ack.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t wire[GROUND_ACK_WIRE_SIZE] = {0};
    const uint8_t expected[GROUND_ACK_WIRE_SIZE] = {
        'W','A','C','K', 1, 2, 10, 64, 0, 0, 0x12, 0x34, 0x56, 0x78
    };
    ground_ack_encode(wire, 2, 10, 64, 0x12345678U, GROUND_ACK_ACCEPTED);
    if (memcmp(wire, expected, sizeof(wire)) != 0) {
        fputs("ground_ack_test: FAIL\n", stderr);
        return 1;
    }
    puts("ground_ack_test: PASS");
    return 0;
}
