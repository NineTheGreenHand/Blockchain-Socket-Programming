// clientA.cpp
// Client A file for the socket programming project

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

#define LOCALHOST "127.0.0.1"             // localhost
#define PORT "25687"                      // server port number
#define MAXDATASIZE 10000                 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// parse the message received from Server M:
void parse_message(char *ret[], char *message) {
    char *p = strtok(message, " ");
    int i = 0;
    while (p != NULL) {
        ret[i++] = p;
        p = strtok(NULL, " ");
    }
}

// prints proper message based on result received:
void print_message(char *results[], char *argv[]) {
    char cmd1[] = "cw";         // check wallet
    char cmd2[] = "txc";        // transfer coins
    char cmd3[] = "txl";        // TXLIST
    char cmd4[] = "stat";       // Statistics
    char success[] = "s";       // Request success
    char fail1[] = "f1";        // user not in the network for check wallet, or no enough balance for sender for transfer coins
    char fail2_s[] = "f2s";     // sender not in the network for transfer coins
    char fail2_r[] = "f2r";     // receiver not in the network for transfer coins
    char fail3[] = "f3";        // both sender and receiver not in the network for transfer coins
    // command was check wallet:
    if (strcmp(results[0], cmd1) == 0) {
        if (strcmp(results[1], success) == 0) {
            printf("The current balance of \"%s\" is: %s alicoins.\n", argv[1], results[2]);
        } else if (strcmp(results[1], fail1) == 0) {
            printf("Unable to proceed with the request as \"%s\" is not part of the network.\n", argv[1]);
        }
    // command was transfer coins:
    } else if (strcmp(results[0], cmd2) == 0) {
        if (strcmp(results[1], success) == 0) {
            printf("\"%s\" successfully transferred %s alicoins to \"%s\".\n", argv[1], argv[3], argv[2]);
            printf("The current balance of \"%s\" is: %s alicoins.\n", argv[1], results[2]);
        } else if (strcmp(results[1], fail1) == 0) {
            printf("\"%s\" was unable to transfer %s alicoins to \"%s\" because of insufficient balance.\n", argv[1], argv[3], argv[2]);
            printf("The current balance of \"%s\" is: %s alicoins.\n", argv[1], results[2]);
        } else if (strcmp(results[1], fail2_s) == 0) {
            printf("Unable to proceed with the transaction as \"%s\" is not part of the network.\n", argv[1]);
        } else if (strcmp(results[1], fail2_r) == 0) {
            printf("Unable to proceed with the transaction as \"%s\" is not part of the network.\n", argv[2]);
        } else if (strcmp(results[1], fail3) == 0) {
            printf("Unable to proceed with the transaction as \"%s\" and \"%s\" are not part of the network.\n", argv[1], argv[2]);
        }
    }
}

int main(int argc, char *argv[]) {
    char message[MAXDATASIZE];      // this is the message we want to sent to server M from command line input
    
    // set several boolean variable to generate proper message sent to server M:
    bool checkwallet = false;
    bool txcoins = false;
    bool txlist = false;

    // this operation is transfer coins:
    // the message format is: "txc sender receiver #coins"
    if (argc == 4) {
        // user1 user2 coins
        sprintf(message, "txc %s %s %s", argv[1], argv[2], argv[3]);
        txcoins = true;
    // one argument, could be check wallet or TXLIST:
    } else {
        string check = "TXLIST";
        // TXLIST command, message is "txl"
        if (argv[1] == check) {
            txlist = true;
             sprintf(message, "txl");
        // check wallet command, message is: "cw username"
        } else {
            checkwallet = true;
             sprintf(message, "cw %s", argv[1]);
        }
    }

    // create socket:
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct sockaddr_in serv_addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("client: socket creation failed.\n");
        return -1;
    }
    
    // set the server side address:
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(25687);
    serv_addr.sin_addr.s_addr = inet_addr(LOCALHOST);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("client: connect failed.\n");
        return -1;
    }

    // print on-screen message, client is up:
    printf("The client A is up and running.\n");

    // send information to Server M:
    if ((numbytes = send(sockfd, message, strlen(message), 0)) == -1) {
        perror("Client A: failed to send message to Server M.");
        exit(1);
    }

    // print the proper on-screen message based on the operation:
    if (checkwallet) {
        printf("\"%s\" sent a balance enquiry request to the main server.\n", argv[1]);
    } else if (txcoins) {
        printf("\"%s\" has requested to transfer %s coins to \"%s\".\n", argv[1], argv[3], argv[2]);
    } else if (txlist) {
        // based on Piazza post @164 and @274:
        printf("ClientA sent a sorted list request to the main server.\n");
    }

    // receive information from Server M:
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1) {
        perror("Client A: failed to receive message from Server M.");
        exit(1);
    }

    // this is the message we get from server M:
    buf[numbytes] = '\0';

    // parse the message and print messages based on the results:
    char *results[10];
    parse_message(results, buf);
    print_message(results, argv);

    close(sockfd);
    return 0; 
}
