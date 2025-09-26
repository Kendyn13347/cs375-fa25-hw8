// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT 8080

static void die(const char *m){perror(m);exit(1);}

int main(int argc, char **argv) {
    const char *msg = (argc > 1) ? argv[1] : "Hello Server";
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) die("socket");

    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);

    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0) die("connect");

    // Send the whole message (no newline required)
    if (send(s, msg, strlen(msg), 0) < 0) die("send");

    char buf[1024];
    ssize_t n = recv(s, buf, sizeof(buf)-1, 0);
    if (n > 0) { buf[n] = 0; printf("%s", buf); }
    close(s);
    return 0;
}
