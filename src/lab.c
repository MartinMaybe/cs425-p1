#include "lab.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
