#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define DATAFILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10
#define RECV_CHUNK 1024

static volatile sig_atomic_t g_exit_requested = 0;
static int g_server_fd = -1;

static void handle_signal(int signo) {
  (void)signo;
  g_exit_requested = 1;
  // If blocked in accept(), closing server fd helps break out on some systems.
  if (g_server_fd != -1) {
    shutdown(g_server_fd, SHUT_RDWR);
    close(g_server_fd);
    g_server_fd = -1;
  }
}

static int send_all(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  size_t total = 0;
  while (total < len) {
    ssize_t n = send(fd, p + total, len - total, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    total += (size_t)n;
  }
  return 0;
}

static int daemonize(void) {
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    exit(0); // parent exits

  if (setsid() < 0)
    return -1;

  pid = fork();
  if (pid < 0)
    return -1;
  if (pid > 0)
    exit(0); // first child exits

  umask(0);
  if (chdir("/") < 0)
    return -1;

  // Redirect stdio to /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd < 0)
    return -1;
  dup2(fd, STDIN_FILENO);
  dup2(fd, STDOUT_FILENO);
  dup2(fd, STDERR_FILENO);
  if (fd > 2)
    close(fd);

  return 0;
}

int main(int argc, char *argv[]) {
  // Will prevent other arugments from beinng passed when compiling the file
  // /file -d and /file Only ones allowed
  bool do_daemon = false;
  if (argc == 2 && strcmp(argv[1], "-d") == 0) {
    do_daemon = true;
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
    return 1;
  }

  openlog("aesdsocket", LOG_PID, LOG_USER);

  // Signals
  // Calls handle signal so that the termianl can interup
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGINT, &sa, NULL) != 0 || sigaction(SIGTERM, &sa, NULL) != 0) {
    syslog(LOG_ERR, "sigaction failed: %s", strerror(errno));
    closelog();
    return 1;
  }
  // If the daemonize function fails it tells us
  if (do_daemon) {
    if (daemonize() != 0) {
      syslog(LOG_ERR, "daemonize failed: %s", strerror(errno));
      closelog();
      return 1;
    }
  }

  // 1) socket()
  g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (g_server_fd < 0) {
    syslog(LOG_ERR, "socket failed: %s", strerror(errno));
    closelog();
    return 1;
  }

  // 2) setsockopt(SO_REUSEADDR)
  int opt = 1;
  if (setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
      0) {
    syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
    close(g_server_fd);
    g_server_fd = -1;
    closelog();
    return 1;
  }

  // 3) bind()
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(PORT);

  if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    syslog(LOG_ERR, "bind failed: %s", strerror(errno));
    close(g_server_fd);
    g_server_fd = -1;
    closelog();
    return 1;
  }

  // 4) listen()
  if (listen(g_server_fd, BACKLOG) < 0) {
    syslog(LOG_ERR, "listen failed: %s", strerror(errno));
    close(g_server_fd);
    g_server_fd = -1;
    closelog();
    return 1;
  }

  while (!g_exit_requested) {
    // 5) accept()
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd =
        accept(g_server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      if (errno == EINTR || g_exit_requested)
        break;
      syslog(LOG_ERR, "accept failed: %s", strerror(errno));
      continue;
    }
    // Logs the client IP address
    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    syslog(LOG_INFO, "Accepted connection from %s", ip);

    // Receive until newline
    char recvbuf[RECV_CHUNK];
    char *line = NULL;
    size_t line_cap = 0;
    size_t line_len = 0;

    bool got_newline = false;
    while (!got_newline) {
      ssize_t n = recv(client_fd, recvbuf, sizeof(recvbuf), 0);
      if (n < 0) {
        if (errno == EINTR)
          continue;
        syslog(LOG_ERR, "recv failed: %s", strerror(errno));
        break;
      }
      if (n == 0) {
        // client closed
        break;
      }

      // grow line buffer
      if (line_len + (size_t)n + 1 > line_cap) {
        size_t newcap = (line_cap == 0) ? 2048 : line_cap;
        while (newcap < line_len + (size_t)n + 1)
          newcap *= 2;
        char *tmp = realloc(line, newcap);
        if (!tmp) {
          syslog(LOG_ERR, "realloc failed");
          break;
        }
        line = tmp;
        line_cap = newcap;
      }

      memcpy(line + line_len, recvbuf, (size_t)n);
      line_len += (size_t)n;
      line[line_len] = '\0';

      if (memchr(recvbuf, '\n', (size_t)n) != NULL) {
        got_newline = true;
      }
    }

    if (line && line_len > 0 && got_newline) {
      // Append received line to file
      int fd = open(DATAFILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd < 0) {
        syslog(LOG_ERR, "open datafile failed: %s", strerror(errno));
      } else {
        ssize_t w = write(fd, line, line_len);
        if (w < 0) {
          syslog(LOG_ERR, "write failed: %s", strerror(errno));
        }
        close(fd);

        // Send back the entire file
        fd = open(DATAFILE, O_RDONLY);
        if (fd < 0) {
          syslog(LOG_ERR, "open datafile for read failed: %s", strerror(errno));
        } else {
          char filebuf[RECV_CHUNK];
          for (;;) {
            ssize_t r = read(fd, filebuf, sizeof(filebuf));
            if (r < 0) {
              if (errno == EINTR)
                continue;
              syslog(LOG_ERR, "read failed: %s", strerror(errno));
              break;
            }
            if (r == 0)
              break; // EOF
            if (send_all(client_fd, filebuf, (size_t)r) != 0) {
              syslog(LOG_ERR, "send failed: %s", strerror(errno));
              break;
            }
          }
          close(fd);
        }
      }
    }

    free(line);
    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", ip);
  }

  // Cleanup
  if (g_server_fd != -1) {
    close(g_server_fd);
    g_server_fd = -1;
  }
  unlink(DATAFILE);
  syslog(LOG_INFO, "Exiting");
  closelog();
  return 0;
}
