#ifndef LAB_H
#define LAB_H
#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>

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

// Layer 2

// Function pointer type for reading bytes from transporter
typedef ssize_t (*read_fn)(void *p, char *buf, size_t len);
// Function pointer type for writing bytes to transporter
typedef ssize_t (*write_fn)(void *p, const char *buf, size_t len);

// Struct for transporter read/write
typedef struct {
    read_fn read;
    write_fn write;
    void *p;
} smtp_t;

int read_line(smtp_t *io, char *out, size_t outsize);
int read_reply(smtp_t *io, reply_t *out);
int expect_reply(smtp_t *io, int expected_code, reply_t *out);
int send_line(smtp_t *io, const char *line);
int send_body(smtp_t *io, const char *body);

// Runs SMPT conversation over transport 
int run_smtp_session(smtp_t *io, const struct smtp_config *cfg, const char *body);

// Layer 1
int parse_reply_line(const char *line, int *code, int *is_final);
int dot_stuff_line(const char *line, size_t linelen, char *out, size_t outsize);

// Layer 3

int connect_to_server(const char *host, const char *port);
// read with TCP socket
ssize_t socket_read_cb(void *p, char *buf, size_t len);
// write with TCP socket
ssize_t socket_write_cb(void *p, const char *buf, size_t len);

// other
int parse_args(int argc, char **argv, struct smtp_config *cfg);
void print_usage(FILE *out);
char* read_stdin_body(void);

#endif // LAB_H
