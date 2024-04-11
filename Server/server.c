#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "jokes.h"

/*Constant Variables*/
#define BACKLOG         10
#define BASE_10         10
#define BUFFER_SIZE     2048
#define ERROR           -1
#define CLIENT_EXIT     2
#define MAX_CLIENTS     100
#define NULL_TERMINATOR '\0'
#define TELL_JOKE       1
#define ZERO            0

/*Enums and Structs*/
typedef enum {false = 0, true = 1} bool;

struct Client {
    struct sockaddr_in address;
    int socketFD;
    int uid;
    char name[INET6_ADDRSTRLEN];
};

/*Global Variables*/
static _Atomic unsigned int client_count = 0;
static int uid = 10;
struct Client* clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

/*Functions Prototypes*/
void addClient(struct Client* client);
void* handleClient(void* arg);
void* getInAddr(struct sockaddr* socketAddress);
void* getInPort(struct sockaddr* socketAddress);
void removeClient(int uid);


int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*Variable Declarations*/
    int socketFD;
    int clientFD;
    int rv;
    int yes;
    char clientAddrStr[INET6_ADDRSTRLEN];
    char* ip = "127.0.0.1";
    char* port = argv[1];
    struct addrinfo hints;
    struct addrinfo* servInfo;
    struct addrinfo* servPtr;
    struct sockaddr_storage clientAddr;
    socklen_t clientAddrSize;
    pthread_t threadID;

    

    /*Set up server hints*/
    memset(&hints, ZERO, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags = AI_PASSIVE; /*Will use my address*/

    if ((rv = getaddrinfo(ip, port, &hints, &servInfo)) != ZERO) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return EXIT_FAILURE;
    }

    /*Loop through all nodes and bind first node that works*/
    for(servPtr = servInfo; servPtr != NULL; servPtr = servPtr->ai_next) {
        if ((socketFD = socket(servPtr->ai_family, servPtr->ai_socktype, 
                servPtr->ai_protocol)) == ERROR) {
            perror("server: socket");
            continue;
        }

        /*Force reuse of port if socket closes*/
        yes = 1;
        if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, 
                &yes, sizeof(int)) == ERROR) {
            perror("server: setsockopt");
            exit(EXIT_FAILURE);
        }

        if (bind(socketFD, servPtr->ai_addr, servPtr->ai_addrlen) == ERROR) {
            close(socketFD);
            perror("sever: bind... will try next addr if any");
            continue;
        }

        /*If we made it here, we're go to good, so break loop*/
        break;
    }

    freeaddrinfo(servInfo); /*No longer needed, so can be freed*/
    if (servPtr == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(EXIT_FAILURE);
    }
    
    if (listen(socketFD, BACKLOG) == ERROR) {
        perror("sever: listen");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "server: waiting for connections...\n");
    memset(clients, 0, sizeof(clients));

    while(true) { 
        /*Server Engine - Main accept() functionality goes here*/
        clientAddrSize = sizeof(clientAddr);
        if ((clientFD = accept(socketFD, (struct sockaddr*)&clientAddr, 
                &clientAddrSize)) == ERROR) {
            perror("server: Unable to accept clinet");
            exit(EXIT_FAILURE);
        }
        
        /*Verify if max number of clients are connected to server*/
        if (client_count == MAX_CLIENTS) {
            fprintf(stdout, 
                    "Max clients reached. Rejected connection from > %s: %d\n",
                    clientAddrStr,
                    *(int*)getInPort((struct sockaddr*)&clientAddr));
            close(clientFD);
            continue;
        }

        /*Let server know who's connecting via terminal*/
        inet_ntop(
            clientAddr.ss_family, 
            getInAddr((struct sockaddr*)&clientAddr),
            clientAddrStr,
            sizeof(clientAddrStr));

        fprintf(
            stdout, 
            "server: %s has connected\n", 
            clientAddrStr);
        fflush(stdout);

        /*Set up clients to be created and handled*/
        struct Client* client = (struct Client*)calloc(1, sizeof(struct Client));
        if (client == NULL) {
            perror("server: Failed to allocate memory for new client.");
            return EXIT_FAILURE;
        }

        client->address = 
            *((struct sockaddr_in*)(getInAddr((struct sockaddr*)&clientAddr)));
        client->socketFD = clientFD;
        client->uid = uid++;

        /*Add client*/
        addClient(client);
        pthread_create(&threadID, NULL, &handleClient, (void*)client);
        sleep(1);
    }
    
    return EXIT_SUCCESS;
}



/*Function Definitions*/
void addClient(struct Client* client) {
    pthread_mutex_lock(&clients_mutex);
    int i = 0;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == NULL) {
            clients[i] = client;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void* handleClient(void* arg) {
    bool needValidChoice;
    bool keepConnectionOpen;
    char buffer[BUFFER_SIZE];
    char* errorPtr;
    int clientChoice;
    int bytesReceived;
    int randomJokeVal;
    struct Client* client = (struct Client*)arg;

    srand(time(NULL));
    client_count++;

    memset(buffer, ZERO, BUFFER_SIZE);
    strncpy(buffer, "Welcome to Link's Joke Server!\n\n", BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = NULL_TERMINATOR;
    if (send(client->socketFD, buffer, BUFFER_SIZE, 0) == ERROR) {
        perror("server: handleClient: send");
        goto closeConnection;
    }

    keepConnectionOpen = true;

    while (keepConnectionOpen) {
        memset(buffer, ZERO, BUFFER_SIZE);
        strncpy(buffer,
        "\nWhat would you like to do?\n1 - Hear a joke\n2 - Exit\n", BUFFER_SIZE);
        buffer[BUFFER_SIZE - 1] = NULL_TERMINATOR;
        if (send(client->socketFD, buffer, BUFFER_SIZE, 0) == ERROR) {
            perror("sever: handleClient: send (Menu Options)");
            goto closeConnection;
        }

        memset(buffer, ZERO, BUFFER_SIZE);

        needValidChoice = true;
        while (needValidChoice) {
            if ((bytesReceived = recv(client->socketFD, buffer, 
                    BUFFER_SIZE - 1, 0)) == ERROR) {
                perror("server: handle: recv");
                goto closeConnection;
            }
            buffer[BUFFER_SIZE - 1] = NULL_TERMINATOR;
            
            clientChoice = strtol(buffer, &errorPtr, BASE_10);

            if (clientChoice == TELL_JOKE || clientChoice == CLIENT_EXIT) {
                needValidChoice = false;
            }
        }

        switch (clientChoice) {
            case TELL_JOKE:
                memset(buffer, ZERO, BUFFER_SIZE);
                randomJokeVal = (rand() % JOKE_SIZE) + 1; 
                strncpy(buffer, jokes[randomJokeVal], BUFFER_SIZE);
                if (send(client->socketFD, buffer, BUFFER_SIZE, 0) == ERROR) {
                    perror("sever: case - TELL_JOKE");
                    keepConnectionOpen = false;
                    goto closeConnection;
                }
                break;
            case CLIENT_EXIT:
                memset(buffer, ZERO, BUFFER_SIZE);
                strncpy(buffer, "GOODBYE!\n", BUFFER_SIZE);
                buffer[BUFFER_SIZE - 1] = NULL_TERMINATOR;
                if (send(client->socketFD, buffer, BUFFER_SIZE, 0) == ERROR) {
                    perror("sever: case - CLIENT_EXIT");
                    goto closeConnection;
                }
                keepConnectionOpen = false;
                break;
            default:
                keepConnectionOpen = false;
                break;
        }
    }

closeConnection:
    close(client->socketFD);
    removeClient(client->uid);
    client = NULL;
    client_count--;
    pthread_detach(pthread_self());
    return NULL;
}


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

void removeClient(int uid) {
    pthread_mutex_lock(&clients_mutex);
    int i = 0;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]->uid == uid) {
            clients[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}
