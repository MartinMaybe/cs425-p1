#include "lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif


int main(int argc, char **argv)
{
    struct smtp_config cfg;
    int rc = parse_args(argc, argv, &cfg);

    // no args check
    if (rc == -1) {
        return 0;
    } 
    // wrong args check
    if (rc != 0) {
        return 1;
    }

    // connect to server
    int fd = connect_to_server(cfg.server, cfg.port);
    if (fd == -1) {
        return 2;
    } 

    smtp_t io = {
        .read = socket_read_cb,
        .write = socket_write_cb,
        .p = &fd,
    };

    reply_t reply;
    if (expect_reply(&io, 220, &reply) != 0) {
        close(fd);
        return 2;
    }

    printf("Recieved greeting: %s", reply.text);

    char *stdin_body = NULL;
    const char *body_to_send = cfg.body;
    if (cfg.body[0] == '\0') {
        stdin_body = read_stdin_body();
        if (!stdin_body) {
            fprintf(stderr, "myapp: failed to read body message from stdin\n");
            close(fd);
            return 2;
        }
        body_to_send = stdin_body;
    }

    int result = 0;
    if (run_smtp_session(&io, &cfg, body_to_send) != 0) {
        result = 2;
    }

    free(stdin_body);
    close(fd);
    return result;
}