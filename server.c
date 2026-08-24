// Pseudo code:
// fd = socket()
// bind(fd, address)
// listen(fd)
// while True:
//  conn_fd = accept(fd)
//  do_something(conn_fd)
//  close(conn_fd)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <assert.h>

const size_t k_max_msg = 4096;

static void msg(const char *msg) {
  fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg) {
  int err = errno;
  fprintf(stderr, "[%d] %s\n", err, msg);
  abort();
}

static int32_t read_full(int fd, char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = read(fd, buf, n);
    if (rv <= 0) {
      return -1;
    }

    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }

  return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
  while (n > 0) {
    ssize_t rv = write(fd, buf, n);
    if (rv <= 0) return -1;

    assert((size_t)rv <= n);
    n -= (size_t)rv;
    buf += rv;
  }

  return 0;
}

satic void do_something(int conn_fd) {
  char rbuf[64] = {};
  ssize_t n = read(conn_fd, rbuf, sizeof(rbuf) - 1);
  
  if (n < 0) {
    msg("read() error");
    return;
  }
  printf("client says : %s\n", rbuf);

  char wbuf[] = "world";
  write(conn_fd, wbuf, strlen(wbuf));
}

static int32_t one_request(int conn_fd) {
  char rbuf[4 + k_max_msg];
  errno = 0;

  int32_t err = read_full(conn_fd, rbuf, 4);
  if (err) {
    msg(errno == 0 ? "EOF" : "read() error");
    return err;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4); // assume little endian
  if (len > k_max_msg) {
    msg("too long");
    return -1;
  }

  err = read_full(conn_fd, &rbuf[4], len);
  if (err) {
    msg("read error()");
    return err;
  }

  printf("client says: %.*s\n", len, &rbuf[4]);
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t)strlen(reply);
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);
  return write_all(conn_fd, wbuf, 4 + len);
}

int main(int argc, const char* argv[]) {
  // AF_INET is for IPv4, AF_INET6 for IPv6; SOCK_STREAM for TCP, SOCK_DGRAM for UDP.
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    die("socket()");
  }

  // We are enabling our socket at fd to be resuable. 
  // Basically, don't prevent my server rebinding to this address
  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};

  addr.sin_family = AF_INET;
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(0);
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

  if (rv) {
    die("bind()");
  }

  rv = listen(fd, SOMAXCONN);
  if (rv) {
    die("listen()");
  }

  while (true) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);

    int conn_fd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);

    if (conn_fd < 0) {
      continue;
    }

    while (true) {
      int32_t err = one_request(conn_fd);
      if (err) {
        break;
      }
    }

    close(conn_fd);
  }

  return 0;
}
