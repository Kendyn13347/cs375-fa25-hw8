// server.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT 8080
#define BACKLOG 128
#define BUFSZ 1024

static volatile sig_atomic_t shutdown_requested = 0;

static void die(const char *msg) { perror(msg); exit(EXIT_FAILURE); }

static void sigchld_handler(int sig) {
    (void)sig;
    int saved = errno;
    // Reap all exited children
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved;
}

static void sigusr1_handler(int sig) {
    (void)sig;
    shutdown_requested = 1; // child asked us to shutdown
}

static void install_handlers(void) {
    struct sigaction sa = {0};

    // Reap children
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;        // restart accept() after SIGCHLD
    if (sigaction(SIGCHLD, &sa, NULL) < 0) die("sigaction(SIGCHLD)");

    // Graceful shutdown trigger from child
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) die("sigaction(SIGUSR1)");
}

static void handle_client(int client_sock) {
    char buf[BUFSZ];
    size_t total = 0;
    int is_shutdown = 0;

    // Chunked read until peer closes (EOF) or we exceed some reasonable cap
    for (;;) {
        ssize_t n = recv(client_sock, buf, sizeof(buf)-1, 0);
        if (n == 0) break;                  // peer closed
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }
        buf[n] = '\0';                      // safe for %s
        total += (size_t)n;

        // Simple command check (case-sensitive): if any chunk contains "shutdown"
        if (strstr(buf, "shutdown")) {
            is_shutdown = 1;
        }
        // Echo back an acknowledgement per chunk (optional)
        // send(client_sock, "ok\n", 3, 0);
    }

    if (is_shutdown) {
        const char *ack = "Server: shutting down gracefully.\n";
        send(client_sock, ack, strlen(ack), 0);
        // Notify parent
        kill(getppid(), SIGUSR1);
    } else {
        const char *resp = "Hello from C server\n";
        send(client_sock, resp, strlen(resp), 0);
    }

    close(client_sock);
    _exit(0);
}

int main(void) {
    install_handlers();

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) die("socket");

    // Allow quick restart after kill:3
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        die("setsockopt(SO_REUSEADDR)");
    // Optional: SO_REUSEPORT (platform-dependent)
    // setsockopt(server_sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr, cli;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("bind");
    if (listen(server_sock, BACKLOG) < 0) die("listen");

    printf("Server listening on 0.0.0.0:%d\n", PORT);

    for (;;) {
        if (shutdown_requested) break;

        socklen_t len = sizeof(cli);
        int client = accept(server_sock, (struct sockaddr*)&cli, &len);
        if (client < 0) {
            if (errno == EINTR) continue;   // interrupted by signal; retry
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); close(client); continue; }
        if (pid == 0) {
            close(server_sock);             // child doesn't need the listen socket
            handle_client(client);
        } else {
            close(client);                  // parent doesn't use client socket
        }
    }

    printf("Server: graceful shutdown.\n");
    close(server_sock);
    return 0;
}
