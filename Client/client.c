#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/*Constant Variables*/
#define BUFFER_SIZE         2048
#define ERROR               -1
#define NULL_TERMINATOR     '\0'
#define ZERO                0

/*Functions Prototypes*/
void* getInAddr(struct sockaddr* socketAddress);
void* getInPort(struct sockaddr* socketAddress);



int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*Variable Declarations*/
    int bytesReceived;
    int socketFD;
    int rv;
    char* ip = "127.0.0.1";
    char* port = argv[1];
    char buffer[BUFFER_SIZE];
    char serverAddrStr[INET6_ADDRSTRLEN];
    struct addrinfo hints;
    struct addrinfo* servInfo;
    struct addrinfo* servPtr;

    /*Set up server hints*/
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_protocol = SOCK_STREAM;

    if ((rv = getaddrinfo(ip, port, &hints, &servInfo)) != ZERO) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    /*Loop through all nodes and bind first node that works*/
    for(servPtr = servInfo; servPtr != NULL; servPtr = servPtr->ai_next) {
        if ((socketFD = socket(servPtr->ai_family, servPtr->ai_socktype, 
                servPtr->ai_protocol)) == ERROR) {
            perror("client: socket");
            continue;
        }

        /*Connect to the server*/
        if (connect(socketFD, servPtr->ai_addr, servPtr->ai_addrlen) == ERROR) {
            close(socketFD);
            perror("client: connect");
            continue;
        }
        
        /*If we made it here, we're go to good, so break loop*/
        break;
    }

    if (servPtr == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return EXIT_FAILURE;
    }

    inet_ntop(servPtr->ai_family, getInAddr((struct sockaddr*)servPtr->ai_addr), 
        serverAddrStr, sizeof(serverAddrStr));
    printf("client: connecting to %s\n", serverAddrStr);
    fflush(stdout);

    freeaddrinfo(servInfo); /*No longer needed*/

    memset(buffer, 0, BUFFER_SIZE);
    if ((bytesReceived = recv(socketFD, buffer, BUFFER_SIZE, 0)) == ERROR) {
        perror("client: recv");
        exit(EXIT_FAILURE);
    } else if (bytesReceived > ZERO) {
        buffer[BUFFER_SIZE - 1] = NULL_TERMINATOR;
        printf("%s\n", buffer);
    }




    close(socketFD);

    return EXIT_SUCCESS;
}


/*Function Definitions*/
void* getInAddr(struct sockaddr* socketAddress) {
    if (socketAddress->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)socketAddress)->sin_addr);
    }
    return &(((struct sockaddr_in6*)socketAddress)->sin6_addr);
}

void* getInPort(struct sockaddr* socketAddress) {
    if (socketAddress->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)socketAddress)->sin_port);
    }
    return &(((struct sockaddr_in6*)socketAddress)->sin6_port);
}
