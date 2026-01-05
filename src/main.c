#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

#define SERVER_PORT 8080
#define BUFFER_SIZE 2048

/* Простые "статические файлы", зашитые в прошивку */

static const char INDEX_HTML[] =
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "  <meta charset=\"utf-8\">\n"
    "  <title>Embedded HTTP server</title>\n"
    "  <link rel=\"stylesheet\" href=\"/style.css\">\n"
    "</head>\n"
    "<body>\n"
    "  <h1>HTTP server in C</h1>\n"
    "  <p>This page is served as a static resource from a simple single threaded server.</p>\n"
    "  <p>Try <a href=\"/image\">/image</a> to see binary content response.</p>\n"
    "</body>\n"
    "</html>\n";

static const char STYLE_CSS[] =
    "body { font-family: Arial, sans-serif; background: #f5f5f5; }\n"
    "h1 { color: #333; }\n"
    "a { color: #007acc; }\n";

/* Просто заглушка под бинарный контент, по сути это просто байты */
static const unsigned char IMAGE_DATA[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, /* GIF89a header */
    0x01, 0x00, 0x01, 0x00,
    0x80, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
    0x21, 0xf9, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x2c, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00,
    0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x3b
};
static const size_t IMAGE_DATA_SIZE = sizeof(IMAGE_DATA);

/* Отправка простого HTTP ответа со строковым телом */

void send_text_response(int client_fd,
                        const char *status_line,
                        const char *content_type,
                        const char *body)
{
    char header[512];
    size_t body_len = strlen(body);

    int header_len = snprintf(
        header,
        sizeof(header),
        "%s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_line,
        content_type,
        body_len
    );

#ifdef _WIN32
    send(client_fd, header, header_len, 0);
    send(client_fd, body, (int)body_len, 0);
#else
    write(client_fd, header, header_len);
    write(client_fd, body, body_len);
#endif
}

/* Отправка бинарного ответа */

void send_binary_response(int client_fd,
                          const char *status_line,
                          const char *content_type,
                          const unsigned char *data,
                          size_t data_len)
{
    char header[512];

    int header_len = snprintf(
        header,
        sizeof(header),
        "%s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_line,
        content_type,
        data_len
    );

#ifdef _WIN32
    send(client_fd, header, header_len, 0);
    send(client_fd, (const char *)data, (int)data_len, 0);
#else
    write(client_fd, header, header_len);
    write(client_fd, data, data_len);
#endif
}

/* Очень простой разбор HTTP запроса, смотрим только первую строку */

void handle_client(int client_fd)
{
    char buffer[BUFFER_SIZE];
#ifdef _WIN32
    int received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
#else
    int received = (int)read(client_fd, buffer, BUFFER_SIZE - 1);
#endif

    if (received <= 0) {
        return;
    }

    buffer[received] = '\0';

    /* Ожидаем что первая строка формата: GET /path HTTP/1.1 */
    char method[8] = {0};
    char path[256] = {0};

    if (sscanf(buffer, "%7s %255s", method, path) != 2) {
        send_text_response(client_fd,
                           "HTTP/1.1 400 Bad Request",
                           "text/plain; charset=utf-8",
                           "Bad Request\n");
        return;
    }

    if (strcmp(method, "GET") != 0) {
        send_text_response(client_fd,
                           "HTTP/1.1 405 Method Not Allowed",
                           "text/plain; charset=utf-8",
                           "Only GET is supported\n");
        return;
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        send_text_response(client_fd,
                           "HTTP/1.1 200 OK",
                           "text/html; charset=utf-8",
                           INDEX_HTML);
    } else if (strcmp(path, "/style.css") == 0) {
        send_text_response(client_fd,
                           "HTTP/1.1 200 OK",
                           "text/css; charset=utf-8",
                           STYLE_CSS);
    } else if (strcmp(path, "/image") == 0) {
        send_binary_response(client_fd,
                             "HTTP/1.1 200 OK",
                             "image/gif",
                             IMAGE_DATA,
                             IMAGE_DATA_SIZE);
    } else {
        const char *not_found =
            "<html><body><h1>404 Not Found</h1></body></html>\n";
        send_text_response(client_fd,
                           "HTTP/1.1 404 Not Found",
                           "text/html; charset=utf-8",
                           not_found);
    }
}

int main(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        return 1;
    }

    if (listen(server_fd, 4) < 0) {
        perror("listen failed");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        return 1;
    }

    printf("HTTP server is listening on port %d\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd =
            accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        handle_client(client_fd);

#ifdef _WIN32
        closesocket(client_fd);
#else
        close(client_fd);
#endif
    }

#ifdef _WIN32
    closesocket(server_fd);
    WSACleanup();
#else
    close(server_fd);
#endif

    return 0;
}
