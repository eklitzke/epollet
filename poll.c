#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXEVENTS 64
#define PORT 9000

static void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl()");
    return;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl()");
  }
}

int main(int argc, char **argv) {
  // create the server socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("socket()");
    return 1;
  }
  int enable = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) ==
      -1) {
    perror("setsockopt()");
    return 1;
  }

  // bind
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(PORT);
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind()");
    return 1;
  }

  // make it nonblocking, and then listen
  set_nonblocking(sock);
  if (listen(sock, SOMAXCONN) < 0) {
    perror("listen()");
    return 1;
  }

  // create the epoll socket
  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    perror("epoll_create1()");
    return 1;
  }

  // mark the server socket for reading, and become edge-triggered
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.data.fd = sock;
  event.events = EPOLLIN | EPOLLET;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1) {
    perror("epoll_ctl()");
    return 1;
  }

  struct epoll_event *events = calloc(MAXEVENTS, sizeof(event));
  for (;;) {
    int nevents = epoll_wait(epoll_fd, events, MAXEVENTS, -1);
    if (nevents == -1) {
      perror("epoll_wait()");
      return 1;
    }
    for (int i = 0; i < nevents; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
          (!(events[i].events & EPOLLIN))) {
        // error case
        fprintf(stderr, "epoll error\n");
        close(events[i].data.fd);
        continue;
      } else if (events[i].data.fd == sock) {
        // server socket; call accept as many times as we can
        for (;;) {
          struct sockaddr in_addr;
          socklen_t in_addr_len = sizeof(in_addr);
          int client = accept(sock, &in_addr, &in_addr_len);
          if (client == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // we processed all of the connections
              break;
            } else {
              perror("accept()");
              return 1;
            }
          } else {
            printf("accepted new connection on fd %d\n", client);
            set_nonblocking(client);
            event.data.fd = client;
            event.events = EPOLLIN | EPOLLET;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &event) == -1) {
              perror("epoll_ctl()");
              return 1;
            }
          }
        }
      } else {
        // client socket; read as much data as we can
        char buf[1024];
        for (;;) {
          ssize_t nbytes = read(events[i].data.fd, buf, sizeof(buf));
          if (nbytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              printf("finished reading data from client\n");
              break;
            } else {
              perror("read()");
              return 1;
            }
          } else if (nbytes == 0) {
            printf("finished with %d\n", events[i].data.fd);
            close(events[i].data.fd);
            break;
          } else {
            fwrite(buf, sizeof(char), nbytes, stdout);
          }
        }
      }
    }
  }
  return 0;
}
