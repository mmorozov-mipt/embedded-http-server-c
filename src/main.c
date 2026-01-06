#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define RECV_BUFFER_SIZE 4096

static void send_response(int client_fd,
                          const char *status,
                          const char *content_type,
                          const void *body,
                          size_t body_len)
{
    char header[512];
    int header_len = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status,
        content_type,
        body_len
    );

    write(client_fd, header, header_len);

    if (body && body_len > 0) {
        write(client_fd, body, body_len);
    }
}

static void handle_root(int client_fd)
{
    const char *html =
        "<!doctype html>"
        "<html><head><meta charset=\"utf-8\"><title>HTTP server in C</title></head>"
        "<body>"
        "<h1>Embedded HTTP server in C</h1>"
        "<p>This page is served as a static response from a simple single threaded server.</p>"
        "<ul>"
        "<li><a href=\"/file\">Text file example</a></li>"
        "<li><a href=\"/image\">Image example</a></li>"
        "</ul>"
        "</body></html>";

    send_response(client_fd, "200 OK", "text/html; charset=utf-8",
                  html, strlen(html));
}

static void handle_text_file(int client_fd)
{
    const char *text =
        "This is a static text response from C HTTP server.\n"
        "It simulates serving a small text file with Content-Type: text/plain.\n";

    send_response(client_fd, "200 OK", "text/plain; charset=utf-8",
                  text, strlen(text));
}

static void handle_image(int client_fd)
{
    const char *image_path = "assets/example.png";
    FILE *f = fopen(image_path, "rb");
    if (!f) {
        const char *msg = "Image not found\n";
        send_response(client_fd, "404 Not Found", "text/plain; charset=utf-8",
                      msg, strlen(msg));
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        const char *msg = "Empty image file\n";
        send_response(client_fd, "500 Internal Server Error", "text/plain; charset=utf-8",
                      msg, strlen(msg));
        return;
    }

    char *buffer = (char *)malloc((size_t)size);
    if (!buffer) {
        fclose(f);
        const char *msg = "Memory allocation failed\n";
        send_response(client_fd, "500 Internal Server Error", "text/plain; charset=utf-8",
                      msg, strlen(msg));
        return;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (read_bytes != (size_t)size) {
        free(buffer);
        const char *msg = "Failed to read image file\n";
        send_response(client_fd, "500 Internal Server Error", "text/plain; charset=utf-8",
                      msg, strlen(msg));
        return;
    }

    send_response(client_fd, "200 OK", "image/png", buffer, (size_t)size);
    free(buffer);
}

static void handle_client(int client_fd)
{
    char buffer[RECV_BUFFER_SIZE];
    ssize_t received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0) {
        close(client_fd);
        return;
    }

    buffer[received] = '\0';

    char method[8];
    char path[256];
    if (sscanf(buffer, "%7s %255s", method, path) != 2) {
        const char *msg = "Bad request\n";
        send_response(client_fd, "400 Bad Request", "text/plain; charset=utf-8",
                      msg, strlen(msg));
        close(client_fd);
        return;
    }

    if (strcmp(method, "GET") != 0) {
        const char *msg = "Only GET is supported\n";
        send_response(client_fd, "405 Method Not Allowed", "text/plain; charset=utf-8",
                      msg, strlen(msg));
        close(client_fd);
        return;
    }

    if (strcmp(path, "/") == 0) {
        handle_root(client_fd);
    } else if (strcmp(path, "/file") == 0) {
        handle_text_file(client_fd);
    } else if (strcmp(path, "/image") == 0) {
        handle_image(client_fd);
    } else {
        const char *msg = "Not found\n";
        send_response(client_fd, "404 Not Found", "text/plain; charset=utf-8",
                      msg, strlen(msg));
    }

    close(client_fd);
}

int main(void)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 8) < 0) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }

    printf("HTTP server listening on port %d\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}
