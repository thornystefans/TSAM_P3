//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections
#define serverID "V_Group_16"

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
  public:
    int sock;              // socket of client connection
    std::string name;           // Limit length of name of client's user
    bool is_group_16;
    std::string ipAddress;
    std::string port;

    Client(int sock, std::string ipAddress) {
        this->sock = sock;
        is_group_16 = false;
        this->ipAddress = ipAddress;
    }

    ~Client(){}            // Virtual destructor defined for base class
};

std::map<int, Client*> clients; // Lookup table for per Client information

std::string removeTokens(char* buffer) {
    std::string str = std::string(buffer);
    if(str.find("\0x01") == 0 && str.compare(str.size() - 1, 1, "\0x04") == 0)
    {
        str.erase(0, 1);
        str.erase(str.size() - 1, str.size());
    }
    return str;
}

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.
int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.
   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients
   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
      return(-1);
   }
   else
   {
      return(sock);
   }
}

std::string addTokens(char* buffer) {
    std::string tokenStr = "";
    std::string str = std::string(buffer);
    std::string soh = "\x01";
    std::string eot = "\x04";

    tokenStr = soh + str + eot;
    return tokenStr;
}

void connectServer(std::string ipAddress, std::string port, fd_set *openSockets, int *maxfds) {
    struct addrinfo hints, *svr;              // Network host entry for server
    struct sockaddr_in serv_addr;             // Socket address for server
    char message[5000];
    int serverSocket;
    int set = 1;                              // Toggle for setsockopt

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(ipAddress.c_str(), port.c_str(), &hints, &svr) != 0) {
        perror("getaddrinfo failed: ");
        exit(0);
    }

    struct hostent *server;
    server = gethostbyname(ipAddress.c_str());

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(port.c_str()));

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0) {
       printf("Failed to set SO_REUSEADDR for port %s\n", port.c_str());
       perror("setsockopt failed: ");
    }

    if(connect(serverSocket, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("Failed to open socket to server: %s\n", ipAddress.c_str());
        perror("Connect failed: ");
        exit(0);
    }

    FD_SET(serverSocket, openSockets);
    *maxfds = std::max(*maxfds, serverSocket);

    std::cout << "Connected to " << server->h_name << std::endl;

    strcpy(message, "LISTSERVERS,V_GROUP_16");
    std::string str = addTokens(message);
    strcpy(message, str.c_str());
        
    send(serverSocket, message, strlen(message), 0);
    // VANTAR TIMA HER

    clients.insert(std::pair<int, Client*>(serverSocket, new Client(serverSocket, ipAddress)));
    clients[serverSocket]->port = port;
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds) {
    // Remove client from the clients list
    clients.erase(clientSocket);

    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    if(*maxfds == clientSocket) {
        for(auto const& p : clients) {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.
    FD_CLR(clientSocket, openSockets);
}

// Checking if our server is connected to 5 servers, returning true if so
// Since our client is a part of the clients map, we subtract 1
bool isFull() {
    if((clients.size() - 1) >= 5) {
        return true;
    }
    return false;
}

// Process command from client on the server

void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, 
                  char *buffer) {
    std::vector<std::string> tokens;
    std::string token;

    // Split command from client into tokens for parsing
    std::istringstream stream(buffer);

    while(std::getline(stream, token, ','))
        tokens.push_back(token);

    if((tokens[0].compare("CONNECT") == 0) && (tokens.size() == 3)) {
        // If server has reached the maximum number of connections (5) it will not connect
        if(!isFull()) {
            std::string ipAddress = tokens[1];
            std::string port = tokens[2];
            connectServer(ipAddress, port, openSockets, maxfds);
        } else {
            strcpy(buffer, "Server has reached the maximum number of connections");
            std::string str = addTokens(buffer);
            strcpy(buffer, str.c_str());
            send(clientSocket, buffer, strlen(buffer), 0);
        }
    }
    else if(tokens[0].compare("LISTSERVERS") == 0) {

          // við að senda á listservers
    }
    else if(tokens[0].compare("LEAVE") == 0) {
      // Close the socket, and leave the socket handling
      // code to deal with tidying up clients etc. when
      // select() detects the OS has torn down the connection.
 
        closeClient(clientSocket, openSockets, maxfds);
    }
    else if(tokens[0].compare("WHO") == 0) {
        std::cout << "Who is logged on" << std::endl;
        std::string msg;

        for(auto const& names : clients) {
            msg += names.second->name + ",";
        }
        // Reducing the msg length by 1 loses the excess "," - which
        // granted is totally cheating.
        send(clientSocket, msg.c_str(), msg.length()-1, 0);
    }
    // This is slightly fragile, since it's relying on the order
    // of evaluation of the if statement.
    else if((tokens[0].compare("MSG") == 0) && (tokens[1].compare("ALL") == 0)) {
        std::string msg;
        for(auto i = tokens.begin()+2;i != tokens.end();i++) {
            msg += *i + " ";
        }

        for(auto const& pair : clients) {
            send(pair.second->sock, msg.c_str(), msg.length(),0);
        }
    }
    else if(tokens[0].compare("MSG") == 0) {
        for(auto const& pair : clients) {
            if(pair.second->name.compare(tokens[1]) == 0) {
                std::string msg;
                for(auto i = tokens.begin()+2;i != tokens.end();i++) {
                    msg += *i + " ";
                }
                send(pair.second->sock, msg.c_str(), msg.length(),0);
            }
        }
    }
    else if(tokens[0].compare("Group16 - hello from the other side")) {
        clients[clientSocket]->is_group_16 = true;
    }
    else {
        std::cout << "Unknown command from client:" << buffer << std::endl;
    }   
}

void serverCommand(int clientSocket, fd_set *openSockets, int *maxfds, 
                  char *buffer) {

    std::vector<std::string> tokens;
    std::string token;

    // Split command from client into tokens for parsing
    std::istringstream stream(buffer);

    while(std::getline(stream, token, ','))
        tokens.push_back(token);

    if(tokens[0].compare("SERVERS") == 0) {
        // svarið við listservers
        // savea upplýsingar um serverana í klasa/map
    }

}

int main(int argc, char* argv[]) {
    bool finished;
    int listenSock;                 // Socket for connections to server
    int clientSock;                 // Socket of connecting client
    fd_set openSockets;             // Current open sockets 
    fd_set readSockets;             // Socket list for select()        
    fd_set exceptSockets;           // Exception socket list
    int maxfds;                     // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025];              // buffer for reading from clients

    if(argc != 2) {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1])); 
    printf("Listening on port: %d\n", atoi(argv[1]));

    if(listen(listenSock, BACKLOG) < 0) {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else {
        // Add listen socket to socket set we are monitoring
        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;

    while(!finished) {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if(n < 0) {
            perror("select failed - closing down\n");
            finished = true;
        }
        else {
            // First, accept  any new connections to the server on the listening socket
            if(FD_ISSET(listenSock, &readSockets) && !isFull()) {
               clientSock = accept(listenSock, (struct sockaddr *)&client,
                                   &clientLen);
               printf("accept***\n");
               // Add new client to the list of open sockets
               FD_SET(clientSock, &openSockets);

               // And update the maximum file descriptor
               maxfds = std::max(maxfds, clientSock);

               // create a new client to store information.
               clients[clientSock] = new Client(clientSock, inet_ntoa(client.sin_addr));

               // Decrement the number of sockets waiting to be dealt with
               n--;

               printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            while(n-- > 0) {
               for(auto const& pair : clients) {
                  Client *client = pair.second;

                  if(FD_ISSET(client->sock, &readSockets)) {
                      // recv() == 0 means client has closed connection
                      if(recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0) {
                          printf("Client closed connection: %d", client->sock);
                          close(client->sock);      

                          closeClient(client->sock, &openSockets, &maxfds);
                      }
                      // We don't check for -1 (nothing received) because select()
                      // only triggers if there is something on the socket for us.
                      else {
                          std::cout << buffer << std::endl;
                          if(client->is_group_16) {
                              clientCommand(client->sock, &openSockets, &maxfds, 
                                        buffer);
                          } else {
                              // server command
                              serverCommand(client->sock, &openSockets, &maxfds, 
                                        buffer);
                          } 
                      }
                  }
               }
            }
        }
    }
}