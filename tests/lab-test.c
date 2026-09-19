#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "harness/unity.h"
#include "../src/lab.h"
#include <arpa/inet.h>

void setUp(void) {}
void tearDown(void) {}

void test_get_greeting(void) {
  char *greeting = get_greeting("Alice");
  TEST_ASSERT_NOT_NULL(greeting);
  TEST_ASSERT_EQUAL_STRING("Hello, Alice!", greeting);
  free(greeting);

  greeting = get_greeting(NULL);
  TEST_ASSERT_NULL(greeting);

  greeting = get_greeting("");
  TEST_ASSERT_NOT_NULL(greeting);
  TEST_ASSERT_EQUAL_STRING("Hello, !", greeting);
  free(greeting);
}

/* ============================================================
 * Layer 1: parse_reply_line
 * ============================================================ */

void test_parse_reply_line_final(void) {
  int code, is_final;
  TEST_ASSERT_EQUAL_INT(0, parse_reply_line("250 Ok", &code, &is_final));
  TEST_ASSERT_EQUAL_INT(250, code);
  TEST_ASSERT_EQUAL_INT(1, is_final);
}

void test_parse_reply_line_continuation(void) {
  int code, is_final;
  TEST_ASSERT_EQUAL_INT(0, parse_reply_line("250-PIPELINING", &code, &is_final));
  TEST_ASSERT_EQUAL_INT(250, code);
  TEST_ASSERT_EQUAL_INT(0, is_final);
}

void test_parse_reply_line_too_short(void) {
  int code, is_final;
  TEST_ASSERT_EQUAL_INT(-1, parse_reply_line("25", &code, &is_final));
}

void test_parse_reply_line_bad_separator(void) {
  int code, is_final;
  TEST_ASSERT_EQUAL_INT(-1, parse_reply_line("250xOk", &code, &is_final));
}

/* ============================================================
 * Layer 1: dot_stuff_line
 * ============================================================ */

void test_dot_stuff_line_normal(void) {
  char out[32];
  int n = dot_stuff_line("hello", 5, out, sizeof(out));
  TEST_ASSERT_EQUAL_INT(5, n);
  TEST_ASSERT_EQUAL_STRING("hello", out);
}

void test_dot_stuff_line_leading_dot(void) {
  char out[32];
  int n = dot_stuff_line(".hello", 6, out, sizeof(out));
  TEST_ASSERT_EQUAL_INT(7, n);
  TEST_ASSERT_EQUAL_STRING("..hello", out);
}

void test_dot_stuff_line_empty(void) {
  char out[32];
  int n = dot_stuff_line("", 0, out, sizeof(out));
  TEST_ASSERT_EQUAL_INT(0, n);
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_dot_stuff_line_overflow(void) {
  char out[3];
  int n = dot_stuff_line("abcdef", 6, out, sizeof(out));
  TEST_ASSERT_EQUAL_INT(-1, n);
}

/* ============================================================
 * parse_args
 * ============================================================ */

void test_parse_args_no_args(void) {
  optind = 1;
  char *argv[] = { "myapp" };
  struct smtp_config cfg;
  int rc = parse_args(1, argv, &cfg);
  TEST_ASSERT_EQUAL_INT(-1, rc);
}

void test_parse_args_missing_to(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "host" };
  struct smtp_config cfg;
  int rc = parse_args(4, argv, &cfg);
  TEST_ASSERT_EQUAL_INT(1, rc);
}

void test_parse_args_missing_from(void) {
  optind = 1;
  char *argv[] = { "myapp", "-t", "you@x.com", "host" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(1, parse_args(4, argv, &cfg));
}

void test_parse_args_crlf_injection(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "-t", "you@x.com",
                    "-s", "bad\r\nRCPT TO:<evil>", "host" };
  struct smtp_config cfg;
  int rc = parse_args(8, argv, &cfg);
  TEST_ASSERT_EQUAL_INT(1, rc);
}

void test_parse_args_crlf_in_from(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "bad\r\nMAIL FROM:<x>", "-t", "you@x.com", "host" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(1, parse_args(6, argv, &cfg));
}

void test_parse_args_crlf_in_to(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "-t", "bad\r\nRCPT TO:<x>", "host" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(1, parse_args(6, argv, &cfg));
}

void test_parse_args_valid(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "-t", "you@x.com", "host" };
  struct smtp_config cfg;
  int rc = parse_args(6, argv, &cfg);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_STRING("me@x.com", cfg.from);
  TEST_ASSERT_EQUAL_STRING("you@x.com", cfg.to);
  TEST_ASSERT_EQUAL_STRING("host", cfg.server);
  TEST_ASSERT_EQUAL_STRING("25", cfg.port);
  TEST_ASSERT_EQUAL_STRING("localhost", cfg.helo_host);
}

void test_parse_args_all_options(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "-t", "you@x.com",
                    "-s", "subj", "-b", "body text", "-p", "2525",
                    "-H", "myhost", "host" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(0, parse_args(14, argv, &cfg));
  TEST_ASSERT_EQUAL_STRING("body text", cfg.body);
  TEST_ASSERT_EQUAL_STRING("2525", cfg.port);
  TEST_ASSERT_EQUAL_STRING("myhost", cfg.helo_host);
}

void test_parse_args_invalid_option(void) {
  optind = 1;
  char *argv[] = { "myapp", "-z", "host" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(1, parse_args(3, argv, &cfg));
}

void test_parse_args_missing_server(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "-t", "you@x.com" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(1, parse_args(5, argv, &cfg));
}

void test_parse_args_extra_argument(void) {
  optind = 1;
  char *argv[] = { "myapp", "-f", "me@x.com", "-t", "you@x.com", "host", "extra" };
  struct smtp_config cfg;
  TEST_ASSERT_EQUAL_INT(1, parse_args(7, argv, &cfg));
}

/* ============================================================
 * read_stdin_body
 * ============================================================ */

void test_read_stdin_body_reads_all(void) {
  int saved_stdin = dup(STDIN_FILENO);
  int fds[2];
  pipe(fds);
  const char *msg = "hello stdin body";
  write(fds[1], msg, strlen(msg));
  close(fds[1]);
  dup2(fds[0], STDIN_FILENO);
  close(fds[0]);

  char *body = read_stdin_body();
  TEST_ASSERT_NOT_NULL(body);
  TEST_ASSERT_EQUAL_STRING(msg, body);
  free(body);

  dup2(saved_stdin, STDIN_FILENO);
  close(saved_stdin);
}

/* ============================================================
 * Layer 2: fake in-memory server harness
 * ============================================================ */

typedef struct {
  const char *script;
  size_t script_len;
  size_t read_pos;
  size_t chunk_size;   /* 0 = unlimited */
  size_t hangup_after; /* SIZE_MAX = never */

  char written[4096];
  size_t written_len;
} fake_server_t;

static void fake_server_init(fake_server_t *fs, const char *script, size_t chunk_size) {
  fs->script = script;
  fs->script_len = strlen(script);
  fs->read_pos = 0;
  fs->chunk_size = chunk_size;
  fs->hangup_after = SIZE_MAX;
  fs->written_len = 0;
  fs->written[0] = '\0';
}

static ssize_t fake_read_cb(void *p, char *buf, size_t len) {
  fake_server_t *fs = (fake_server_t *)p;

  if (fs->read_pos >= fs->hangup_after) {
    return 0;
  }
  size_t remaining = fs->script_len - fs->read_pos;
  if (remaining == 0) {
    return 0;
  }
  size_t to_copy = remaining;
  if (fs->chunk_size != 0 && to_copy > fs->chunk_size) {
    to_copy = fs->chunk_size;
  }
  if (to_copy > len) {
    to_copy = len;
  }
  memcpy(buf, fs->script + fs->read_pos, to_copy);
  fs->read_pos += to_copy;
  return (ssize_t)to_copy;
}

static ssize_t fake_write_cb(void *p, const char *buf, size_t len) {
  fake_server_t *fs = (fake_server_t *)p;
  size_t space = sizeof(fs->written) - 1 - fs->written_len;
  size_t to_copy = (len < space) ? len : space;
  memcpy(fs->written + fs->written_len, buf, to_copy);
  fs->written_len += to_copy;
  fs->written[fs->written_len] = '\0';
  return (ssize_t)len;
}
//new
void test_read_reply_malformed_line(void) {
  fake_server_t fs;
  fake_server_init(&fs, "ab\r\n", 0); /* valid line, but only 2 chars: fails parse_reply_line */
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  reply_t reply;
  TEST_ASSERT_EQUAL_INT(-1, read_reply(&io, &reply));
}

void test_read_line_handles_empty_line(void) {
  fake_server_t fs;
  fake_server_init(&fs, "\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  char out[64];
  TEST_ASSERT_EQUAL_INT(0, read_line(&io, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_read_line_truly_empty_line(void) {
  fake_server_t fs;
  fake_server_init(&fs, "\n", 0);  /* \n with nothing before it: n is 0 when we hit it */
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  char out[64];
  TEST_ASSERT_EQUAL_INT(0, read_line(&io, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("", out);
}

void test_read_line_no_cr(void) {
  fake_server_t fs;
  fake_server_init(&fs, "hello\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  char out[64];
  TEST_ASSERT_EQUAL_INT(5, read_line(&io, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("hello", out);
}

static void make_test_cfg(struct smtp_config *cfg) {
  cfg->from = "me@x.com";
  cfg->to = "you@x.com";
  cfg->subject = "hello";
  cfg->body = "";
  cfg->port = "25";
  cfg->helo_host = "localhost";
  cfg->server = "host";
}

static ssize_t fail_write_cb(void *p, const char *buf, size_t len) {
  (void)p; (void)buf; (void)len;
  return -1;
}

void test_send_line_too_long(void) {
  smtp_t io = { fake_read_cb, fake_write_cb, NULL };
  char long_line[700];
  memset(long_line, 'a', sizeof(long_line) - 1);
  long_line[sizeof(long_line) - 1] = '\0';
  TEST_ASSERT_EQUAL_INT(-1, send_line(&io, long_line));
}

void test_send_line_write_failure(void) {
  smtp_t io = { fake_read_cb, fail_write_cb, NULL };
  TEST_ASSERT_EQUAL_INT(-1, send_line(&io, "HELLO"));
}

void test_send_body_strips_existing_cr(void) {
  fake_server_t fs;
  fake_server_init(&fs, "", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  TEST_ASSERT_EQUAL_INT(0, send_body(&io, "Hello\r\nWorld\r\n"));
  TEST_ASSERT_NULL(strstr(fs.written, "\r\r\n"));
}

void test_send_body_line_too_long(void) {
  fake_server_t fs;
  fake_server_init(&fs, "", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  char body[700];
  memset(body, 'x', sizeof(body) - 2);
  body[sizeof(body) - 2] = '\n';
  body[sizeof(body) - 1] = '\0';
  TEST_ASSERT_EQUAL_INT(-1, send_body(&io, body));
}

void test_send_body_line_send_failure(void) {
  smtp_t io = { fake_read_cb, fail_write_cb, NULL };
  TEST_ASSERT_EQUAL_INT(-1, send_body(&io, "hello\nworld\n"));
}

void test_send_body_no_trailing_newline(void) {
  fake_server_t fs;
  fake_server_init(&fs, "", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  TEST_ASSERT_EQUAL_INT(0, send_body(&io, "no newline at end"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "no newline at end\r\n"));
}

typedef struct { int fail_after; int calls; } countdown_t;
static ssize_t countdown_write_cb(void *p, const char *buf, size_t len) {
  countdown_t *c = (countdown_t *)p;
  c->calls++;
  if (c->calls > c->fail_after) return -1;
  return (ssize_t)len;
}

void test_send_body_final_dot_send_failure(void) {
  countdown_t c = { .fail_after = 1, .calls = 0 }; /* body line ok, "." fails */
  smtp_t io = { fake_read_cb, countdown_write_cb, &c };
  TEST_ASSERT_EQUAL_INT(-1, send_body(&io, "only line\n"));
}

void test_send_body_blank_line(void) {
  fake_server_t fs;
  fake_server_init(&fs, "", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  TEST_ASSERT_EQUAL_INT(0, send_body(&io, "Line one.\n\nLine three.\n"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "Line one.\r\n\r\nLine three.\r\n"));
}

#define HAPPY_SCRIPT \
  "250 helo ok\r\n250 mail ok\r\n250 rcpt ok\r\n354 go ahead\r\n250 data ok\r\n221 bye\r\n"

/* ============================================================
 * run_smtp_session: happy paths
 * ============================================================ */

void test_session_happy_path(void) {
  fake_server_t fs;
  fake_server_init(&fs, HAPPY_SCRIPT, 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Hello world.\n");

  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "HELO localhost\r\n"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "MAIL FROM:<me@x.com>\r\n"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "RCPT TO:<you@x.com>\r\n"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "DATA\r\n"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "\r\n.\r\n"));
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "QUIT\r\n"));
}

void test_session_multiline_reply(void) {
  fake_server_t fs;
  fake_server_init(&fs,
    "250-smtp.example.com\r\n250-PIPELINING\r\n250 SIZE 10240000\r\n"
    "250 mail ok\r\n250 rcpt ok\r\n354 go ahead\r\n250 data ok\r\n221 bye\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_session_reply_arrives_in_chunks(void) {
  fake_server_t fs;
  fake_server_init(&fs, HAPPY_SCRIPT, 1); /* one byte per read() */
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_body_with_leading_dot_is_stuffed(void) {
  fake_server_t fs;
  fake_server_init(&fs, HAPPY_SCRIPT, 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Line one.\n.A dotted line.\nLine three.\n");
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "\r\n..A dotted line.\r\n"));
}

/* ============================================================
 * run_smtp_session: reply the buffer cannot hold
 * ============================================================ */

void test_reply_too_large_for_buffer(void) {
  char big[600];
  memset(big, 'A', sizeof(big) - 3);
  big[sizeof(big) - 3] = '\r';
  big[sizeof(big) - 2] = '\n';
  big[sizeof(big) - 1] = '\0';

  fake_server_t fs;
  fake_server_init(&fs, big, 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };

  reply_t reply;
  int rc = read_reply(&io, &reply);
  TEST_ASSERT_EQUAL_INT(-1, rc);
}

typedef struct {
  fake_server_t fs;
  int fail_after;
  int write_calls;
} session_fail_ctx_t;

static ssize_t session_read_cb(void *p, char *buf, size_t len) {
  session_fail_ctx_t *c = (session_fail_ctx_t *)p;
  return fake_read_cb(&c->fs, buf, len);
}

static ssize_t session_write_cb(void *p, const char *buf, size_t len) {
  session_fail_ctx_t *c = (session_fail_ctx_t *)p;
  c->write_calls++;
  if (c->write_calls > c->fail_after) return -1;
  return fake_write_cb(&c->fs, buf, len);
}

void test_session_send_failure_at_each_write(void) {
  /* Full success needs 11 writes: HELO, MAIL, RCPT, DATA, 4 headers,
   * 1 body line, the "." terminator, QUIT. Failing write k+1 exercises
   * the send-failure branch at whichever stage that write belongs to. */
  for (int fail_after = 0; fail_after <= 10; fail_after++) {
    session_fail_ctx_t c;
    fake_server_init(&c.fs, HAPPY_SCRIPT, 0);
    c.fail_after = fail_after;
    c.write_calls = 0;
    smtp_t io = { session_read_cb, session_write_cb, &c };
    struct smtp_config cfg;
    make_test_cfg(&cfg);

    int rc = run_smtp_session(&io, &cfg, "Body line.\n");
    TEST_ASSERT_EQUAL_INT(-1, rc);
  }
}

/* ============================================================
 * run_smtp_session: hangs up mid-session
 * ============================================================ */

void test_session_hangup_mid_session(void) {
  fake_server_t fs;
  fake_server_init(&fs, HAPPY_SCRIPT, 0);
  fs.hangup_after = strlen("250 helo ok\r\n"); /* dies right after HELO's reply */
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");

  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "HELO"));
  TEST_ASSERT_NULL(strstr(fs.written, "QUIT"));
}

/* ============================================================
 * run_smtp_session: wrong status code at each stage
 * ============================================================ */

void test_session_wrong_code_helo(void) {
  fake_server_t fs;
  fake_server_init(&fs, "550 no\r\n250 mail ok\r\n250 rcpt ok\r\n354 go\r\n250 ok\r\n221 bye\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "HELO"));
  TEST_ASSERT_NULL(strstr(fs.written, "MAIL FROM"));
}

void test_session_wrong_code_mail_from(void) {
  fake_server_t fs;
  fake_server_init(&fs, "250 helo ok\r\n550 no\r\n250 rcpt ok\r\n354 go\r\n250 ok\r\n221 bye\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "MAIL FROM"));
  TEST_ASSERT_NULL(strstr(fs.written, "RCPT TO"));
}

void test_session_wrong_code_rcpt_to(void) {
  fake_server_t fs;
  fake_server_init(&fs, "250 helo ok\r\n250 mail ok\r\n550 no\r\n354 go\r\n250 ok\r\n221 bye\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "RCPT TO"));
  TEST_ASSERT_NULL(strstr(fs.written, "DATA"));
}

void test_session_wrong_code_data_354(void) {
  fake_server_t fs;
  fake_server_init(&fs, "250 helo ok\r\n250 mail ok\r\n250 rcpt ok\r\n550 no\r\n250 ok\r\n221 bye\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "DATA\r\n"));
  TEST_ASSERT_NULL(strstr(fs.written, "From:"));
}

void test_session_wrong_code_message_accepted(void) {
  fake_server_t fs;
  fake_server_init(&fs, "250 helo ok\r\n250 mail ok\r\n250 rcpt ok\r\n354 go\r\n550 no\r\n221 bye\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "\r\n.\r\n")); /* body was sent */
  TEST_ASSERT_NULL(strstr(fs.written, "QUIT"));
}

void test_session_wrong_code_quit(void) {
  fake_server_t fs;
  fake_server_init(&fs, "250 helo ok\r\n250 mail ok\r\n250 rcpt ok\r\n354 go\r\n250 ok\r\n550 no\r\n", 0);
  smtp_t io = { fake_read_cb, fake_write_cb, &fs };
  struct smtp_config cfg;
  make_test_cfg(&cfg);

  int rc = run_smtp_session(&io, &cfg, "Body.\n");
  TEST_ASSERT_EQUAL_INT(-1, rc);
  TEST_ASSERT_NOT_NULL(strstr(fs.written, "QUIT\r\n"));
}
//new
void test_connect_to_server_resolution_failure(void) {
  int fd = connect_to_server("this.hostname.should.not.exist.invalid", "25");
  TEST_ASSERT_EQUAL_INT(-1, fd);
}

void test_connect_to_server_success_and_refusal(void) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  TEST_ASSERT_TRUE(listen_fd >= 0);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0; /* OS picks a free port */
  TEST_ASSERT_EQUAL_INT(0, bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)));

  socklen_t addrlen = sizeof(addr);
  TEST_ASSERT_EQUAL_INT(0, getsockname(listen_fd, (struct sockaddr *)&addr, &addrlen));
  int port = ntohs(addr.sin_port);
  TEST_ASSERT_EQUAL_INT(0, listen(listen_fd, 1));

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  int client_fd = connect_to_server("127.0.0.1", port_str);
  TEST_ASSERT_TRUE(client_fd >= 0);
  close(client_fd);
  close(listen_fd); /* now nothing is listening on this port */

  int fail_fd = connect_to_server("127.0.0.1", port_str);
  TEST_ASSERT_EQUAL_INT(-1, fail_fd);
}

void test_socket_read_write_cb(void) {
  int fds[2];
  TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));

  TEST_ASSERT_EQUAL_INT(2, (int)socket_write_cb(&fds[0], "hi", 2));

  char buf[8] = {0};
  TEST_ASSERT_EQUAL_INT(2, (int)socket_read_cb(&fds[1], buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("hi", buf);

  close(fds[0]);
  close(fds[1]);
}

void test_read_stdin_body_large_input_triggers_growth(void) {
  int saved_stdin = dup(STDIN_FILENO);
  int fds[2];
  pipe(fds);

  size_t size = 5000; /* exceeds the initial 4096 capacity */
  char *data = malloc(size);
  memset(data, 'z', size);
  write(fds[1], data, size);
  close(fds[1]);
  free(data);

  dup2(fds[0], STDIN_FILENO);
  close(fds[0]);

  char *body = read_stdin_body();
  TEST_ASSERT_NOT_NULL(body);
  TEST_ASSERT_EQUAL_INT((int)size, (int)strlen(body));
  free(body);

  dup2(saved_stdin, STDIN_FILENO);
  close(saved_stdin);
}

void test_read_stdin_body_read_error(void) {
  int saved_stdin = dup(STDIN_FILENO);
  close(STDIN_FILENO); /* reading now fails with EBADF */

  char *body = read_stdin_body();
  TEST_ASSERT_NULL(body);

  dup2(saved_stdin, STDIN_FILENO);
  close(saved_stdin);
}

/* ============================================================
 * main
 * ============================================================ */

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_get_greeting);

  RUN_TEST(test_parse_reply_line_final);
  RUN_TEST(test_parse_reply_line_continuation);
  RUN_TEST(test_parse_reply_line_too_short);
  RUN_TEST(test_parse_reply_line_bad_separator);

  RUN_TEST(test_dot_stuff_line_normal);
  RUN_TEST(test_dot_stuff_line_leading_dot);
  RUN_TEST(test_dot_stuff_line_empty);
  RUN_TEST(test_dot_stuff_line_overflow);

  RUN_TEST(test_parse_args_no_args);
  RUN_TEST(test_parse_args_missing_to);
  RUN_TEST(test_parse_args_crlf_injection);
  RUN_TEST(test_parse_args_crlf_in_from);
  RUN_TEST(test_parse_args_crlf_in_to);
  RUN_TEST(test_parse_args_valid);
  RUN_TEST(test_parse_args_all_options);
  RUN_TEST(test_parse_args_invalid_option);
  RUN_TEST(test_parse_args_missing_server);
  RUN_TEST(test_parse_args_extra_argument);
  RUN_TEST(test_parse_args_missing_from);

  RUN_TEST(test_read_stdin_body_reads_all);
  RUN_TEST(test_read_reply_malformed_line);
  RUN_TEST(test_read_stdin_body_large_input_triggers_growth);
  RUN_TEST(test_read_stdin_body_read_error);
  RUN_TEST(test_read_line_handles_empty_line);
  RUN_TEST(test_read_line_truly_empty_line);
  RUN_TEST(test_read_line_no_cr);

  RUN_TEST(test_session_happy_path);
  RUN_TEST(test_session_multiline_reply);
  RUN_TEST(test_session_reply_arrives_in_chunks);
  RUN_TEST(test_body_with_leading_dot_is_stuffed);

  RUN_TEST(test_send_line_too_long);
  RUN_TEST(test_send_line_write_failure);
  RUN_TEST(test_send_body_strips_existing_cr);
  RUN_TEST(test_send_body_line_too_long);
  RUN_TEST(test_send_body_line_send_failure);
  RUN_TEST(test_send_body_no_trailing_newline);
  RUN_TEST(test_send_body_final_dot_send_failure);
  RUN_TEST(test_send_body_blank_line);

  RUN_TEST(test_reply_too_large_for_buffer);

  RUN_TEST(test_connect_to_server_resolution_failure);
  RUN_TEST(test_connect_to_server_success_and_refusal);

  RUN_TEST(test_socket_read_write_cb);

  RUN_TEST(test_session_hangup_mid_session);

  RUN_TEST(test_session_wrong_code_helo);
  RUN_TEST(test_session_wrong_code_mail_from);
  RUN_TEST(test_session_wrong_code_rcpt_to);
  RUN_TEST(test_session_wrong_code_data_354);
  RUN_TEST(test_session_wrong_code_message_accepted);
  RUN_TEST(test_session_wrong_code_quit);
  RUN_TEST(test_session_send_failure_at_each_write);

  return UNITY_END();
}