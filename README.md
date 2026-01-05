# embedded-http-server-c

Single threaded HTTP server in C designed for low resource devices (ESP32 class).
Serves a few static files directly from memory without using a filesystem.

## Project description

This project demonstrates how to implement a minimal HTTP server in pure C
that can run on a small device with limited resources.

The server:

- listens on a configurable TCP port (8080 by default)
- handles simple `GET` requests
- serves three static resources:
  - `/` or `/index.html` - HTML page
  - `/style.css` - CSS stylesheet
  - `/image` - small binary image (GIF stub)
- sends correct `Content-Type` and `Content-Length` headers
- closes connection after each response
- uses a single thread and blocking I/O

All content is embedded into the binary as constant arrays which makes this
approach suitable for microcontrollers where access to filesystem is limited.

## Build

### Requirements

- C compiler (clang or gcc)
- CMake 3.10+
- POSIX sockets (Linux, macOS) or Winsock2 (Windows)

### Build on macOS / Linux

```bash
mkdir build
cd build
cmake ..
make
