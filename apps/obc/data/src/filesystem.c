#include "filesystem.h"

#include <stdio.h>
#include <string.h>

#include "obc_data_protocol.h"

static void send_error(OBC_Roles_t requester)
{
    data_read_reply_t reply = { .status = -1, .is_last = 1};
    IPC_send(requester, (const uint8_t *)&reply, sizeof(reply));
}

void filesystem_stream_file(const char *path, OBC_Roles_t requester)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        send_error(requester);
        return;
    }

    uint32_t offset = 0;
    for(;;) {
        data_read_reply_t reply = { .status = 0, .offset = offset };
        size_t n = fread(reply.payload, 1, DATA_CHUNK_SIZE, f);
        reply.length = (uint16_t)n;

        /* 
        fread() returns fewer bytes than asked for only at EOF (feof)
        or on a read error (ferrer) -- either way, this is the last chunk we'll be able to send.
        */
       reply.is_last = (n < DATA_CHUNK_SIZE) ? 1 : 0;

       IPC_send(requester, (const uint8_t *)&reply, sizeof(reply));

       offset += (uint32_t)n;
       if (reply.is_last) break;
    }

    fclose(f);
}