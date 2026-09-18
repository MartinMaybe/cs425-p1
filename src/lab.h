#ifndef LAB_H
#define LAB_H
#include <stdio.h>

#define LINE_MAX_LEN 512

/** * @brief Returns a greeting message.
 *
 * This function returns a string that contains a greeting message.
 * The string is allocated with malloc and should be freed by the caller.
 * @param name The name to include in the greeting.
 * @return A greeting string.
 */
char* get_greeting(const char* restrict name);

struct smtp_config {
    const char *from;
    const char *to;
    const char *subject;
    const char *body;
    const char *port;
    const char *helo_host;
    const char *server;
};


typedef struct {
    int code;
    char text[2048];
} reply_t;

int read_reply(int fd, reply_t *out);

int parse_args(int argc, char **argv, struct smtp_config *cfg);

void print_usage(FILE *out);

int connect_to_server(const char *host, const char *port);

int expect_reply(int fd, int expected_code, reply_t *out);

int send_line(int fd, const char *line);

int send_body(int fd, const char *body);

char* read_stdin_body(void);

#endif // LAB_H
