# Multi-Threaded HTTP Server

This project is a multi-threaded HTTP server implemented in C++. It supports basic HTTP functionalities, including handling `GET` and `POST` requests, serving static files, and responding with gzip-compressed data when requested.

## Features

- **Multi-threaded request handling**: Uses a thread pool to manage incoming client requests efficiently.
- **HTTP Methods Supported**:
  - `GET` requests to serve static content.
  - `POST` requests to store files.
- **Path-based functionalities**:
  - `/echo/<text>`: Returns the text provided in the URL.
  - `/user-agent`: Returns the `User-Agent` header sent by the client.
  - `/files/<filename>`:
    - `GET`: Serves the requested file from the specified directory.
    - `POST`: Saves uploaded content as a file in the directory.
- **Gzip compression support**: If a client requests `gzip` encoding, the server compresses the response.

## Prerequisites

- **C++ Compiler** (g++ recommended)
- **CMake** (for build automation)
- **zlib** (for gzip compression)
- **Linux/macOS** (or Windows with WSL)

## Installation

1. Clone the repository:
   ```sh
   git clone <repository-url>
   cd <repository>
   ```
2. Install dependencies:
   ```sh
   sudo apt-get install zlib1g-dev   # Debian/Ubuntu
   sudo yum install zlib-devel       # CentOS/RHEL
   ```
3. Build the project:
   ```sh
   mkdir build && cd build
   cmake ..
   ```

## Usage

Run the server with an optional directory for file storage:
```sh
./server --directory <path>
```
Example:
```sh
./server --directory /home/user/files
```
The server starts listening on **port 4221**.

## API Endpoints

### `GET /`
Returns a simple `200 OK` response.

### `GET /echo/<text>`
Returns `<text>` as plain text. If `gzip` encoding is accepted, returns a compressed response.

### `GET /user-agent`
Returns the `User-Agent` header sent by the client.

### `GET /files/<filename>`
Retrieves the file from the server's directory.

### `POST /files/<filename>`
Uploads and saves the file to the server's directory.


