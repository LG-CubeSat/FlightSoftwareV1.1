#include "filesystem.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "obc_data_protocol.h"

#define IPC_SEND_MAX_RETRIES 20
#define IPC_SEND_RETRY_DELAY_USEC 2000 // 2ms

/* IPC_send is deliberately fail-fast (see its own comment in obc_ipc.c) --
   it does not retry a connect() failure, since most callers want that (a
   dead/not-yet-up target shouldn't be waited on forever). But a fast
   streaming loop like this one can outrun the receiver's IPC_BACKLOG
   (5 pending connections) if the receiver is doing real work per chunk --
   a real bug this hit during testing: the receiver's accept() couldn't
   keep up, IPC_send failed silently (this function never checked its
   return value), and the dropped chunk was sometimes the final is_last
   one, leaving the reader waiting forever for a chunk that would never
   arrive. A full backlog is transient, not permanent, so retrying with a
   short delay (not IPC_send's own unconditional fail-fast) is the right
   fix here specifically. */
static int ipc_send_retrying(OBC_Roles_t dest, const uint8_t *data, uint16_t length)
{
    for (int attempt = 0; attempt < IPC_SEND_MAX_RETRIES; attempt++) {
        if (IPC_send(dest, data, length) >= 0) return 0;
        usleep(IPC_SEND_RETRY_DELAY_USEC);
    }
    return -1;
}

static void send_error(OBC_Roles_t requester)
{
    data_read_reply_t reply = { .status = -1, .is_last = 1};
    ipc_send_retrying(requester, (const uint8_t *)&reply, sizeof(reply));
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

       if (ipc_send_retrying(requester, (const uint8_t *)&reply, sizeof(reply)) != 0) {
           fprintf(stderr, "[FILESYSTEM] gave up sending chunk at offset %u to role %d after %d retries\n",
                   offset, requester, IPC_SEND_MAX_RETRIES);
           break;
       }

       offset += (uint32_t)n;
       if (reply.is_last) break;
    }

    fclose(f);
}

void filesystem_write_chunk(const char *path, uint32_t offset, uint16_t length,
                             const uint8_t *payload, OBC_Roles_t requester)
{
    data_write_ack_t ack = { .status = -1 };

    FILE *f = fopen(path, offset == 0 ? "wb" : "ab");
    if (f != NULL) {
        size_t written = fwrite(payload, 1, length, f);
        fclose(f);
        if (written == length) {
            ack.status = 0;
        }
    }

    ipc_send_retrying(requester, (const uint8_t *)&ack, sizeof(ack));
}