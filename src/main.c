#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void send_file(int client_fd, const char *path, const char *content_type) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        const char *not_found =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "File not found";
        send(client_fd, not_found, strlen(not_found), 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char header[256];
    sprintf(header,
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n\r\n",
            content_type, size);

    send(client_fd, header, strlen(header), 0);

    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_fd, buffer, bytes, 0);
    }

    fclose(file);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    printf("HTTP server is running on http://localhost:%d\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);

        char req[2048];
        read(client_fd, req, sizeof(req));

        if (strncmp(req, "GET /image", 10) == 0) {
            send_file(client_fd, "assets/example.png", "image/png");
        } else {
            const char *html =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<h1>HTTP server in C</h1>"
                "<p>Open <b>/image</b> to view an embedded PNG image</p>";

            send(client_fd, html, strlen(html), 0);
        }

        close(client_fd);
    }
}
