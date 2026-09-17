#ifndef LAB_H
#define LAB_H
#include <stdio.h>

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

int parse_args(int argc, char **argv, struct smtp_config *cfg);

void print_usage(FILE *out);

int connect_to_server(const char *host, const char *port);



#endif // LAB_H
