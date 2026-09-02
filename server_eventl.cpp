// New server code with event loop

// Pseudo code:
// while running:
//    want_read  = [...]  # socket fds
//    want_write = [...]  # socket fds
//    can_read, can_write = wait_for_readiness(want_read, want_write)
//    for fd in can_read:
//      data = read_nb(fd)  # Non blocking. Only consume from buffer
//      handle_data(fd, data)  # Application logic without IO
//    for fd in can_write:
//      data = pending_data(fd)
//      n = write_nb(fd, data)
//      data_written(fd, n)

// Working
// -------
// So first of all we need to have something that is non-blocking. We want to make sure
// multiple connections can write and read at the same time without blocking anything. 
// Sure, we can use multithreading, but that increases overhead and also memory usage.
// So to overcome all this, we use THE EVENT LOOP
//
// We use non-blocking ways of reading and writing to the kernel buffer. 

#include <vector>
#include <iostream>
#include <cstdint>
#include <cassert>
#include <fcntl.h>
#include <cerrno>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

static void msg(const char *msg) {
  std::cerr << msg << std::endl;
}

static void die(const char *msg) {
  int err = errno;
  std::cerr << "[" << err << "] " << msg << std::endl;
  abort();
}

// Each Conn represents one client connection
//
// `fd` -> This is basically the identifier for the connection
// `want_read` -> Does the connection want to read from the kernel buffer?
// `want_write` -> Does the connection want to write to the kernel buffer?
// `want_close` -> Does the connection want to kill itself?
//
// `incoming` -> This is a buffer that stores the data that is being read.
// `outgoing` -> This buffer stores the data to be sent.
//
// The reason we use these two buffers, is because TCP doesn't ensure that
// the entire data is sent properly over the server. Sometimes we might receive
// half of the data. And because we do not want to block what we do instead is 
// store whatever we have received in the *incoming* buffer. Similarly, for 
// writing, if we cannot write all the bytes, then we write some of it and 
// store the remaning in the *outgoing* buffer.
typedef struct Conn {
  int fd = -1;

  // shows the intention
  bool want_read = false;
  bool want_write = false;
  bool want_close = false;

  // buffered input and output
  std::vector<uint8_t> incoming; // data to be parsed
  std::vector<uint8_t> outgoing; // data to be sent from the connection
}Conn;

// Helper function to append to incoming
static void buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
  buf.insert(buf.end(), data, data + len);
}

// Helper function to consume bytes from the front of a buffer
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
  assert(n <= buf.size());
  buf.erase(buf.begin(), buf.begin() + n);
}

static void fd_set_nonblock(int fd) {
  errno = 0;

  // Getting the flags of the this particular file descriptor
  // F_GETFL gives the flags set for this file descriptor
  int flags = fcntl(fd, F_GETFL, 0);
  assert(flags >= 0);
  
  // Then add the NON BLOCKING flag to the particular file descriptor
  // For this we use F_SETFL
  int rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  assert(rv >= 0);
}

static std::vector<Conn *> fd2conn;

static void handle_accept(int fd) {
  while (true) {
    int conn_fd = accept(fd, NULL, NULL);

    if (conn_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      die("accept()");
    }

    // Setup new connection
    fd_set_nonblock(conn_fd);

    Conn* conn = new Conn();
    conn->fd = conn_fd;
    conn->want_read = true;

    if (fd2conn.size() <= (size_t)conn_fd) {
      fd2conn.resize(conn_fd + 1, nullptr);
    }

    fd2conn[conn_fd] = conn;
  }
}

int main (int argc, char *argv[]) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);

  int val = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

  struct sockaddr_in addr = {};

  addr.sin_family = AF_INET; // IPv4
  addr.sin_port = htons(1234);
  addr.sin_addr.s_addr = htonl(0);
  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
  assert(rv == 0);

  rv = listen(fd, SOMAXCONN);
  assert(rv == 0);
  fd_set_nonblock(fd);

  while (true) {
    // SERVER LOOP
  }

  return 0;
}

