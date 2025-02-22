#include <iostream>
#include <cstdlib>
#include <string>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <queue>
#include <condition_variable>
#include <zlib.h>

// Constants
#define PORT 4221
#define BUFFER_SIZE 4096
#define CONNECTION_BACKLOG 10
#define THREAD_POOL_SIZE 4

// Global Variables
std::string directory;
std::mutex cout_mutex;
std::queue<int> client_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;
bool server_running = true;

// Utility Functions

// Function to split a string
std::vector<std::string> split(const std::string &str, const std::string &delim) {
    std::vector<std::string> tokens;
    size_t start = 0, end;
    while ((end = str.find(delim, start)) != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delim.length();
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

// Function to parse HTTP headers
std::map<std::string, std::string> parse_headers(const std::string &request) {
    std::map<std::string, std::string> headers;
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) return headers;
    
    std::string headers_part = request.substr(0, pos);
    std::vector<std::string> lines = split(headers_part, "\r\n");

    for (size_t i = 1; i < lines.size(); ++i) {
        size_t delimiter_pos = lines[i].find(": ");
        if (delimiter_pos != std::string::npos) {
            headers[lines[i].substr(0, delimiter_pos)] = lines[i].substr(delimiter_pos + 2);
        }
    }
    return headers;
}

// Function to extract path from request
std::string get_path(std::string &request) {
    auto req = split(request, "\r\n");
    return split(req[0], " ")[1];
}

// Function for encoding data
std::string gzip_compress(const std::string &data) {
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw std::runtime_error("deflateInit2 failed while compressing.");
    }
    zs.next_in = (Bytef *)data.data();
    zs.avail_in = data.size();
    int ret;
    char outbuffer[32768];
    std::string outstring;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Exception during zlib compression: (" + std::to_string(ret) + ") " + (zs.msg ? zs.msg : ""));
    }
    return outstring;
}

// Handle Request Function
void handle_request(int client)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_rec = recv(client, buffer, sizeof(buffer) - 1, 0);
    if (bytes_rec <= 0)
    {
        close(client);
        return;
    }

    buffer[bytes_rec] = '\0';
    std::string request(buffer);

    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "Request: " << request << "\n";

    std::string response;
    bool isPathFound = true;
    std::string path = get_path(request);
    std::vector<std::string> path_parts = split(path, "/");
    std::map<std::string, std::string> headers = parse_headers(request);

    if (request.starts_with("GET"))
    {
        if (path == "/")
        {
            response = "HTTP/1.1 200 OK\r\n\r\n";
        }
        else if (path_parts[1] == "echo" && headers.count("Accept-Encoding")){
            std::string encodedTypes = headers["Accept-Encoding"];
            std::string gzipText; 
            if(encodedTypes.find("gzip")!= std::string::npos){
                gzipText = gzip_compress(path_parts[2]); 
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: gzip\r\nContent-Length: " + std::to_string(gzipText.size()) + "\r\n\r\n" + gzipText;
            }
            else 
                response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
        }
        else if (path_parts[1] == "echo" && path_parts.size() > 2)
        {
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(path_parts[2].size()) + "\r\n\r\n" + path_parts[2];
        }
        else if (path_parts[1] == "user-agent")
        {
            std::string user_agent = headers["User-Agent"];
            response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(user_agent.size()) + "\r\n\r\n" + user_agent;
        }
        else if (path_parts[1] == "files" && path_parts.size() > 2)
        {
            std::string filepath = directory + "/" + path_parts[2];
            std::ifstream file(filepath, std::ios::binary);
            if (file.good())
            {
                std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(fileContent.size()) + "\r\n\r\n" + fileContent;
            }
            else
            {
                isPathFound = false;
            }
        }
        else
        {
            isPathFound = false;
        }
    }
    else if (request.starts_with("POST") && path_parts[1] == "files" && path_parts.size() > 2)
    {
        std::string filepath = directory + "/" + path_parts[2];
        size_t body_pos = request.find("\r\n\r\n");
        if (body_pos != std::string::npos)
        {
            body_pos += 4;
            std::string body = request.substr(body_pos);
            std::ofstream outfile(filepath, std::ios::binary);
            if (outfile)
            {
                outfile.write(body.c_str(), body.size());
                response = "HTTP/1.1 201 Created\r\n\r\n";
            }
            else
            {
                isPathFound = false;
            }
        }
        else
        {
            isPathFound = false;
        }
    }
    else
    {
        isPathFound = false;
    }

    if (!isPathFound)
    {
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
    }

    send(client, response.c_str(), response.size(), 0);
    close(client);
}

// Thread Worker Function
void worker_thread()
{
    while (server_running)
    {
        int client;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, []
                          { return !client_queue.empty() || !server_running; });

            if (!server_running && client_queue.empty())
            {
                return;
            }

            client = client_queue.front();
            client_queue.pop();
        }

        handle_request(client);
    }
}

// Server Initialization
int main(int argc, char **argv)
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    directory = "/default";
    if (argc >= 3 && std::string(argv[1]) == "--directory")
    {
        directory = argv[2];
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        std::cerr << "Failed to bind to port " << PORT << "\n";
        return 1;
    }

    if (listen(server_fd, CONNECTION_BACKLOG) != 0)
    {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < THREAD_POOL_SIZE; ++i)
    {
        workers.emplace_back(worker_thread);
    }

    std::cout << "Server is listening on port " << PORT << "...\n";

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (server_running)
    {
        int client = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client < 0)
        {
            std::cerr << "accept failed\n";
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            client_queue.push(client);
        }
        queue_cv.notify_one();
    }

    for (auto &worker : workers)
    {
        worker.join();
    }

    close(server_fd);
    return 0;
}
