#include "lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

char *get_greeting(const char *restrict name)
{
  if (name == NULL)
  {
    return NULL;
  }

  // Allocate memory for the greeting message
  int length = snprintf(NULL, 0, "Hello, %s!", name);
  if (length < 0) // GCOVR_EXCL_START
  {
    return NULL; // snprintf failed
  } // GCOVR_EXCL_STOP

  //Casting is safe here because we know length is non-negative
  size_t alloc_size = (size_t) length + 1; // +1 for the null terminator
  char *greeting = malloc( alloc_size);


  if (greeting == NULL) // GCOVR_EXCL_START
  {
    return NULL; // Memory allocation failed
  }  // GCOVR_EXCL_STOP


  // Create the greeting message
  snprintf(greeting, alloc_size, "Hello, %s!", name);

  return greeting;
}

static int has_crlf(const char *s) {
    return s != NULL && strpbrk(s, "\r\n") != NULL;
}

void print_usage(FILE *out) {
  fprintf(out, 
  "Usage: myapp -f <from> -t <to> [-s subject] [-b body] [-p port]\n"
  "        [-H helo-host] <server>\n"
  "\n"
  " -f <from>       envelope sender, for example you@example.com\n"
  " -t <to>         envelope recipient\n"
  " -s <subject>    subject line (default: empty)\n"
  " -b <body>       message body (default: read from stdin)\n"
  " -p <port>       port or service name (default: 25)\n"
  " -H <helo-host>  host name sent with HELO (default: localhost)\n"
  " <server>        host name or address of the mail server\n"
  );
}

int parse_args(int argc, char **argv, struct smtp_config *cfg) {
  if (argc == 1) {
    print_usage(stdout);
    return -1;
  }

  cfg->from = NULL;
  cfg->to = NULL;
  cfg->subject = "";
  cfg->body = "";
  cfg->port = "25"; // default to 25, may need 587 / 2525
  cfg->helo_host = "localhost";
  cfg->server = NULL;

  int opt;
  
  while ((opt = getopt(argc, argv, "f:t:s:b:p:H:")) != -1) {
    switch (opt) {
      case 'f':
            cfg->from = optarg;
            break;
      case 't':
            cfg->to = optarg;
            break;
      case 's':
            cfg->subject = optarg;
            break;
      case 'b':
            cfg->body = optarg;
            break;
      case 'p':
            cfg->port = optarg;
            break;
      case 'H':
            cfg->helo_host = optarg;
            break;
      default:
            print_usage(stderr);
            return 1;
    }
  }

  // check missing/incorrect args
  if (optind >= argc) {
    fprintf(stderr, "myapp: missing <server>\n");
    print_usage(stderr);
    return 1;
  }
  if (optind + 1 < argc) {
    fprintf(stderr, "myapp: unexpected extra argument '%s'\n", argv[optind + 1]);
    print_usage(stderr);
    return 1;
  }
  cfg->server = argv[optind];

  if (!cfg->from || !cfg->to) {
    fprintf(stderr, "myapp: missing required arg -f <from> | -t <to>\n");
    print_usage(stderr);
    return 1;
  }

  // check for bare CR or LF
  if (has_crlf(cfg->from) || has_crlf(cfg->to) || has_crlf(cfg->subject)) {
    fprintf(stderr, "myapp: detected bare CR/LF in address or subject\n");
    print_usage(stderr);
    return 1;
  }

  return 0;
}

// connecting to server
int connect_to_server(const char *host, const char *port) {
  struct addrinfo hints, *res, *rp;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  int gai_err = getaddrinfo(host, port, &hints, &res);
  if (gai_err != 0) {
    fprintf(stderr, "myapp: could not resolve %s: %s\n", 
            host, gai_strerror(gai_err));
    return -1;
  }

  int fd = -1;
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd == -1) {
      continue; // try next connection
    }
    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break; // worked, save this connection
    }

    // fail to connect, close
    close(fd);
    fd = -1;
  }

  freeaddrinfo(res);

  if (fd == -1) {
    fprintf(stderr, "myapp: could not connect to %s:%s\n", host, port);
    return -1;
  }

  return fd;
}

ssize_t socket_read_cb(void *ctx, char *buf, size_t len) {
  int fd = *(int *)ctx;
  return read(fd, buf, len);
}

ssize_t socket_write_cb(void *ctx, const char *buf, size_t len) {
  int fd = *(int *)ctx;
  return write(fd, buf, len);
}

// Read Lines

int read_line(smtp_t *io, char *out, size_t outsize) {
  size_t n = 0;
  char ch;

  for (;;) {
    ssize_t r = io->read(io->p, &ch, 1);
    if (r<= 0) {
      return -1;
    }
    if (ch == '\n') {
      if (n > 0 && out[n-1] == '\r') {
        n--;
      } 
      out[n] = '\0';
      return (int)n;
    }
    if (n + 1 >= outsize) {
      return -1;
    }
    out[n++] = ch;
  }
  
}

int read_reply(smtp_t *io, reply_t *out) {
    char line[LINE_MAX_LEN];
    out->text[0] = '\0';

    for (;;) {
        int len = read_line(io, line, sizeof(line));
        if (len < 0) return -1; /* too short to have a valid code+sep */

        strncat(out->text, line, sizeof(out->text) - strlen(out->text) - 1);
        strncat(out->text, "\n", sizeof(out->text) - strlen(out->text) - 1);

        int code, is_final;
        if (parse_reply_line(line, &code, &is_final) != 0) {
            return -1; /* malformed reply line */
        }
        if (is_final) {
            out->code = code;
            return 0;
        }
    }
}

int dot_stuff_line(const char *line, size_t linelen, char *out, size_t outsize) {
    int n;
    if (linelen > 0 && line[0] == '.') {
        n = snprintf(out, outsize, ".%.*s", (int)linelen, line);
    } else {
        n = snprintf(out, outsize, "%.*s", (int)linelen, line);
    }
    if (n < 0 || (size_t)n >= outsize) {
        return -1;
    }
    return n;
}

int parse_reply_line(const char *line, int *code, int *is_final) {
    if (strlen(line) < 4) {
        return -1;
    }
    char sep = line[3];
    if (sep != ' ' && sep != '-') {
        return -1;
    }
    char code_buf[4] = { line[0], line[1], line[2], '\0' };
    *code = atoi(code_buf);
    *is_final = (sep == ' ') ? 1 : 0;
    return 0;
}

int expect_reply(smtp_t *io, int expected_code, reply_t *out) {
  if (read_reply(io, out) != 0) {
    fprintf(stderr, "myapp: failed to read server reply\n");
    return -1;
  }

  if (out->code != expected_code) {
    fprintf(stderr, "myapp: expected %d but server replied: %s",
            expected_code, out->text);
    return -1;
  }

  return 0;
}

int send_line(smtp_t *io, const char *line) {
  char buf[LINE_MAX_LEN];
  int n = snprintf(buf, sizeof(buf), "%s\r\n", line);
  if (n < 0 || (size_t)n >= sizeof(buf)) {
    fprintf(stderr, "myapp: command too long: %s\n", line);
    return -1;
  }
  ssize_t total = 0;
  while ((size_t)total < (size_t)n) {
    ssize_t written = io->write(io->p, buf + total, (size_t)n - (size_t)total);
    if (written <= 0) {
        fprintf(stderr, "myapp: failed to send command\n");
        return -1;
      }
      total += written;
  }
  return 0;
}

int send_body(smtp_t *io, const char *body) {
  const char *line_start = body;

  while (*line_start != '\0') {
    const char *newline = strchr(line_start, '\n');
    size_t linelen = newline ? (size_t)(newline - line_start) : strlen(line_start);

    if (linelen > 0 && line_start[linelen - 1] == '\r') {
      linelen--;
    }

    char out_line[LINE_MAX_LEN];
    if (dot_stuff_line(line_start, linelen, out_line, sizeof(out_line)) < 0) {
      fprintf(stderr, "myapp: body line too long\n");
      return -1;
    }
    if (send_line(io, out_line) != 0) {
      return -1;
    }

    if (!newline) {
      break;
    } 
    line_start = newline + 1;
  }

  if (send_line(io, ".") != 0) {
    return -1;
  }
  
  return 0;
}

char *read_stdin_body(void) {
    size_t capacity = 4096;
    size_t used = 0;
    char *buf = malloc(capacity);
    if (!buf) {
        return NULL;
    }

    for (;;) {
        if (used == capacity) {
            size_t new_capacity = capacity * 2;
            char *bigger = realloc(buf, new_capacity);
            if (!bigger) {
                free(buf);
                return NULL;
            }
            buf = bigger;
            capacity = new_capacity;
        }

        ssize_t n = read(STDIN_FILENO, buf + used, capacity - used);
        if (n < 0) {
            free(buf);
            return NULL;
        }
        if (n == 0) {
            break; /* EOF */
        }
        used += (size_t)n;
    }

    /* need room for the NUL terminator */
    if (used == capacity) {
        char *bigger = realloc(buf, capacity + 1);
        if (!bigger) {
            free(buf);
            return NULL;
        }
        buf = bigger;
    }
    buf[used] = '\0';

    return buf;
}

// Runs the whole SMTP conversation. 
int run_smtp_session(smtp_t *io, const struct smtp_config *cfg,
                     const char *body) {
    reply_t reply;
    char cmd[LINE_MAX_LEN];

    snprintf(cmd, sizeof(cmd), "HELO %s", cfg->helo_host);
    if (send_line(io, cmd) != 0) return -1;
    if (expect_reply(io, 250, &reply) != 0) return -1;
    printf("HELO ok: %s", reply.text);

    snprintf(cmd, sizeof(cmd), "MAIL FROM:<%s>", cfg->from);
    if (send_line(io, cmd) != 0) return -1;
    if (expect_reply(io, 250, &reply) != 0) return -1;
    printf("MAIL FROM ok: %s", reply.text);

    snprintf(cmd, sizeof(cmd), "RCPT TO:<%s>", cfg->to);
    if (send_line(io, cmd) != 0) return -1;
    if (expect_reply(io, 250, &reply) != 0) return -1;
    printf("RCPT TO ok: %s", reply.text);

    if (send_line(io, "DATA") != 0) return -1;
    if (expect_reply(io, 354, &reply) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "From: %s", cfg->from);
    if (send_line(io, cmd) != 0) return -1;
    snprintf(cmd, sizeof(cmd), "To: %s", cfg->to);
    if (send_line(io, cmd) != 0) return -1;
    snprintf(cmd, sizeof(cmd), "Subject: %s", cfg->subject);
    if (send_line(io, cmd) != 0) return -1;
    if (send_line(io, "") != 0) return -1;

    if (send_body(io, body) != 0) return -1;
    if (expect_reply(io, 250, &reply) != 0) return -1;
    printf("DATA ok: %s", reply.text);

    if (send_line(io, "QUIT") != 0) return -1;
    if (expect_reply(io, 221, &reply) != 0) return -1;
    printf("QUIT ok: %s", reply.text);

    return 0;
}