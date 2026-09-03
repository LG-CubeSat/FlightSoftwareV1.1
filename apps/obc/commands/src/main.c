#include <stdio.h>

#include "relay.h"
#include "ingest.h"

int main(void) {
    printf("[OBC COMMAND P] Program started.\n");

    /*
    init stuff here
    */
    ingest_thread_init();
    relay_thread_init();

    /*
    Set off the threads
    */

    ingest_thread(NULL);
    relay_thread(NULL);


    /*
    Clean up
    */

    return 0;
}
