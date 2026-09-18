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

    reply_t reply;
    if (expect_reply(fd, 220, &reply) != 0) {
        close(fd);
        return 2;
    }

    printf("Recieved greeting: %s", reply.text);

    // HELO 
    char helo_cmd[LINE_MAX_LEN];
    snprintf(helo_cmd, sizeof(helo_cmd), "HELO %s", cfg.helo_host);
    if (send_line(fd, helo_cmd) != 0) {
        close(fd);
        return 2;
    }
    if (expect_reply(fd, 250, &reply) != 0) {
        close(fd);
        return 2;
    }
    printf("HELO ok: %s", reply.text);

    // MAIL FROM
    char mail_from_cmd[LINE_MAX_LEN];
    snprintf(mail_from_cmd, sizeof(mail_from_cmd), "MAIL FROM:<%s>", cfg.from);
    if (send_line(fd, mail_from_cmd) != 0) {
        close(fd);
        return 2;
    }
    if (expect_reply(fd, 250, &reply) != 0) {
        close(fd);
        return 2;
    }
    printf("MAIL FROM ok: %s", reply.text);

    // RCPT TO
    char rcpt_to_cmd[LINE_MAX_LEN];
    snprintf(rcpt_to_cmd, sizeof(rcpt_to_cmd), "RCPT TO:<%s>", cfg.to);
    if (send_line(fd, rcpt_to_cmd) != 0) {
        close(fd);
        return 2;
    }
    if (expect_reply(fd, 250, &reply) != 0) {
        close(fd);
        return 2;
    }
    printf("RCPT TO ok: %s", reply.text);

    // DATA
    if (send_line(fd, "DATA") != 0) {
        close(fd);
        return 2;
    }
    if (expect_reply(fd, 354, &reply) != 0) {
        close(fd);
        return 2;
    }

    char *stdin_body = NULL;
    const char *body_to_send = cfg.body;

    // body not given, read stdin
    if (cfg.body[0] == '\0') {
        stdin_body = read_stdin_body();
        if (!stdin_body) {
            fprintf(stderr, "myapp: failed to read body message from stdin\n");
            close(fd);
            return 2;
        }
        body_to_send = stdin_body;
    }

    char header_line[LINE_MAX_LEN];
    snprintf(header_line, sizeof(header_line), "From: %s", cfg.from);
    if (send_line(fd, header_line) != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }
    snprintf(header_line, sizeof(header_line), "To: %s", cfg.to);
    if (send_line(fd, header_line) != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }
    snprintf(header_line, sizeof(header_line), "Subject: %s", cfg.subject);
    if (send_line(fd, header_line) != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }
    if (send_line(fd, "") != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }

    if (send_body(fd, body_to_send) != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }

    if (expect_reply(fd, 250, &reply) != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }

    printf("DATA ok: %s", reply.text);

    if (send_line(fd, "QUIT") != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }
    if (expect_reply(fd, 221, &reply) != 0) {
        free(stdin_body);
        close(fd);
        return 2;
    }
    printf("QUIT ok: %s", reply.text);

    free(stdin_body);
    close(fd);
    return 0;
}