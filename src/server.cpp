#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc, char **argv)
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    std::cerr << "Failed to create server socket\n";
    return 1;
  }
  //
  // // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    std::cerr << "setsockopt failed\n";
    return 1;
  }

  // Creating a socket
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0)
  {
    std::cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  std::cout << "Waiting for a client to connect...\n";

  // Accepting the incoming client request and sending response
  // previous code line ---
  // accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  
  // New code --- 
  int client = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
  // Creating a buffer to store the incoming HTTP request
  // and then uses recv to read the request from the client into this buffer.
  std::string buffer(1024, '\0');
  size_t bytes_rec = recv(client, &buffer[0], buffer.size(), 0);
  if (bytes_rec < 0)
  {
    std::cerr << "recv failed\n";
    return 1;
  }
  // After successfully receiving the request, it prints
  // the request to the console for debugging purposes.
  std::cout << "Request: " << buffer << "\n";
  // checks if the request starts with the line GET / HTTP/1.1\r\n, which indicates a request for the root path. If it does,
  // it prepares a 200 OK response. Otherwise, it prepares a 404 Not Found response.
  std::string response;
  // MARK: Handle buffers
  if (buffer.starts_with("GET / HTTP/1.1\r\n"))
  {
    response = "HTTP/1.1 200 OK\r\n\r\n";
  }
  else
  {
    response = "HTTP/1.1 404 Not Found\r\n\r\n";
  }
  // Send the response to client
  send(client, response.c_str(), response.size(), 0);
  std::cout << "Client connected\n";

  close(server_fd);

  return 0;
}
