// serverM.cpp
// Main file for the socket programming project

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
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

#define UDPPORT "24687"                 // UDP port number to connect backend servers A, B, and C
#define TCPPORTA "25687"                // TCP port number to connect with client A
#define TCPPORTB "26687"                // TCP port number to conenct with client B
#define SERVER_A_PORT "21687"           // UDP port number for server A
#define SERVER_B_PORT "22687"           // UDP port number for server B
#define SERVER_C_PORT "23687"           // UDP port number for server C
#define LOCALHOST "127.0.0.1"           // localhost
#define MAXDATASIZE 10000               // max number of bytes we can get at once
#define BACKLOG 10                      // how many pending connections queue will hold
#define LOGFILE "alichain.txt"          // sorted list of logs based on seq number

// get sockaddr, IPv4 or IPv6: (from beej's guide)
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// parse and split the message based on words:
// parsed based on space " "
void parse_message(char *ret[], char *message) {
    char *word = strtok(message, " ");
    int i = 0;
    while (word != NULL) {
        ret[i++] = word;
        word = strtok(NULL, " ");
    }
}

// parse and split the message based on lines:
// parsed based on new line "\n"
void parse_log(char *ret[], char *message) {
    char *line = strtok(message, "\n");
    int i = 0;
    while (line != NULL) {
        ret[i++] = line;
        line = strtok(NULL, "\n");
    }
}

// prints proper message based on result received:
void print_message(char *results[], const char *port) {
    char cmd1[] = "cw";         // check wallet
    char cmd2[] = "txc";        // transfer coins
    char cmd3[] = "txl";        // TXLIST
    // command was check wallet:
    if (strcmp(results[0], cmd1) == 0) {
        printf("The main server received input=\"%s\" from the client using TCP over port %s.\n", results[1], port);
    // command was transfer coins:
    } else if (strcmp(results[0], cmd2) == 0) {
        printf("The main server received from \"%s\" to transfer %s coins to \"%s\" using TCP over port %s.\n", results[1], results[3], results[2], port);
    // command was TXLIST, base on Piazza post @292, print "A TXLIST request has been received."
    } else if (strcmp(results[0], cmd3) == 0) {
        printf("A TXLIST request has been received.\n");
    }
}

// return the seq number of the given log entry:
int get_seq(string log) {
    stringstream ss(log);
    string seq;
    ss >> seq;
    return atoi(seq.c_str());
}

// comparsion function used for sort, to sort the lines
// based on seq number in increasing order:
bool sort_fun(string s1, string s2) {
    return get_seq(s1) < get_seq(s2);
}

// write the sorted transaction log based on seq number:
// logA[0], logB[0], logC[0] contains the number of logs they have eg. 4 means 4 logs
// For example, if logA[0] = x, then logA[1]...logA[x] are the logs server A has
// Firstly, merge all log entries into a vector<string>.
// Then, sort this vector based on first number which is the seq entry.
// Finally, we write into the LOGFILE which is "alichain.txt".
void write_sorted_list_log(char *logA[], char *logB[], char *logC[]) {
    // get number of entries for each logs from backend server A, B, and C
    int logNumA = atoi(logA[0]);
    int logNumB = atoi(logB[0]);
    int logNumC = atoi(logC[0]);
    int totalLog = logNumA + logNumB + logNumC;     // total number of logs should be in "alichain.txt"
    vector<string> logs;

    // merge logs from backend server A, B, and C:
    for (int i = 1; i <= logNumA; i++) {
        string logEntryA = "";
        logEntryA += logA[i];
        logs.push_back(logEntryA);
    }
    for (int i = 1; i <= logNumB; i++) {
        string logEntryB = "";
        logEntryB += logB[i];
        logs.push_back(logEntryB);
    }
    for (int i = 1; i <= logNumC; i++) {
        string logEntryC = "";
        logEntryC += logC[i];
        logs.push_back(logEntryC);
    }

    // sort the log vector based on first seq number:
    sort(logs.begin(), logs.end(), sort_fun);

    // write into "alichain.txt"
    ofstream file(LOGFILE);
    for (int i = 0; i < totalLog - 1; i++) {
        file << logs[i];
        file << "\n";
    }
    file << logs[totalLog - 1];
    file.close();
}

// code used directly from beej's guide:
void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) >0);
    errno = saved_errno;
}

// Initialize a TCP socket with given port number.
// Code below is modified based on beej's guide:
int initTCP(const char *port) {
    int sockfd, rv;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(LOCALHOST, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can:
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL) {
        fprintf(stderr, "Server M: TCP socket failed to bind.\n");
        exit(1);
    }

    // listen for incoming connections:
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    return sockfd;
} 

// Initialize an UDP socket with given port number.
// Code below is modified based on beej's guide:
int initUDP(const char *port) {
    int sockfd, rv;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(LOCALHOST, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can:
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "Server M: UDP socket failed to bind.\n");
        return 2;
    }
    
    freeaddrinfo(servinfo); // all done with this structure

    return sockfd;
}

// With given message (parsed ver.) received from client, send the message to backend server with
// given port number, and store results from the backend server.
void udp_send_recv(char *parsed_message[], char *result, const char *port, int sockfd, char *server) {
    int numbytes, rv;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    memset(&hints, 0, sizeof hints);

    // get address of backend servers A, B or C:
    if ((rv = getaddrinfo(LOCALHOST, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddinfo: %s\n", gai_strerror(rv));
        return;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "server: failed to create socket.\n");
        return;
    }

    // reconstruct the message from the parsed_message to send back to backend server A, B, and C:
    char message[MAXDATASIZE];
    // "cw username"
    if (strcmp(parsed_message[0], "cw") == 0) {
        sprintf(message, "cw %s", parsed_message[1]);
    // "txc sender receiver #coins"
    } else if (strcmp(parsed_message[0], "txc") == 0) {
        sprintf(message, "txc %s %s %s", parsed_message[1], parsed_message[2], parsed_message[3]);
    // "txl"
    } else if (strcmp(parsed_message[0], "txl") == 0) {
        sprintf(message, "txl");
    // "wlog" for write new transaction entry operation, "wlog seq sender receiver #coins"
    } else if (strcmp(parsed_message[0], "wlog") == 0) {
        sprintf(message, "wlog %s %s %s %s", parsed_message[1], parsed_message[2], parsed_message[3], parsed_message[4]);
    }

    // send request to backend server:    
    if ((numbytes = sendto(sockfd, message, strlen(message), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("failed to send message to backend server.\n");
        exit(1);
    } 

    // first info in the message indicates the operation the client wish to perform, no on-screen message for TXLIST:
    if (strcmp(parsed_message[0], "cw") == 0 || strcmp(parsed_message[0], "txc") == 0) {
        printf("The main server sent a request to server %s.\n", server);
    }

    // receive response from backend server:
    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, result, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("failed to receive message from backend server.\n");
        exit(1);
    }

    // the result we get:
    result[numbytes] = '\0';

    // print message based on the operation:
    if (strcmp(parsed_message[0], "cw") == 0) {
        printf("The main server received transactions from Server %s using UDP over port %s.\n", server, port);
    } else if (strcmp(parsed_message[0], "txc") == 0) {
        printf("The main server received feedback from server %s using UDP over port %s.\n", server, port);
    }
}

// connect with client A or client B with TCP, sent query to backend servers based on the operation the 
// client wishes to perform, and generate the result message to return back to the client after all is done:
void tcp_client_operation(int new_fd, int sockfd_UDP, const char *TCPPORT, char *client, int serverNum) {
    char receive[MAXDATASIZE];                     // message received from client
    char receive_UDP_A[MAXDATASIZE];               // message received from backend server A
    char receive_UDP_B[MAXDATASIZE];               // message received from backend server B
    char receive_UDP_C[MAXDATASIZE];               // message received from backend server C
    char *parsed_message[10];                      // parse the message from client
    int numbytes;

    // receive message from client:
    if ((numbytes = recv(new_fd, receive, sizeof receive, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    // message received from the client:
    receive[numbytes] = '\0';

    // With the message received from Client A, we forward the message to Server A, Server B, and Server C:
    // parse the message first:
    parse_message(parsed_message, receive);
    print_message(parsed_message, TCPPORT);

    // Send message to, and receive result from backend server A, B, and C:
    char serverA[] = "A";
    char serverB[] = "B";
    char serverC[] = "C";
    udp_send_recv(parsed_message, receive_UDP_A, SERVER_A_PORT, sockfd_UDP, serverA);
    udp_send_recv(parsed_message, receive_UDP_B, SERVER_B_PORT, sockfd_UDP, serverB);
    udp_send_recv(parsed_message, receive_UDP_C, SERVER_C_PORT, sockfd_UDP, serverC);

    // parse result message get from backend servers, and generate and send message back to client
    char send_message[MAXDATASIZE];
    char *parseA[1000];
    char *parseB[1000];
    char *parseC[1000];
    // for check wallet and transfer coins, the message segment are separated with space, " "
    if (strcmp(parsed_message[0], "cw") == 0 || strcmp(parsed_message[0], "txc") == 0) {         
        parse_message(parseA, receive_UDP_A);
        parse_message(parseB, receive_UDP_B);
        parse_message(parseC, receive_UDP_C);
    // for TXLIST, the message segment are separated with "\n" 
    // (as they are log entries, except the first one: number of logs)
    } else if (strcmp(parsed_message[0], "txl") == 0) {     
        parse_log(parseA, receive_UDP_A);
        parse_log(parseB, receive_UDP_B);
        parse_log(parseC, receive_UDP_C);
    }

    // if the command was check wallet:
    if (strcmp(parsed_message[0], "cw") == 0) {
        // There will be two cases for information in the parseA, parseB, and parseC:
        // 1. [found, summation of balance of user];
        // 2. [notfound]
        // First, if user is found in the server, first entry would be "found", and the second entry is 
        //        the summation of coins that user has received and sent at server. For examle, if user
        //        received 100 coins and sent 20 coins, the second entry should be 100 + (-20) = 80
        // Second, if user is not found in the server, first entry would be "notfound", there will be no
        //        second entry in the parseA, parseB, or parseC 
        int count = 0;          // how many servers not found user
        int balance = 1000;     // initial balance is 1000

        // count number of log files the user is not found, and add summation of transactions of the user if found:
        if (strcmp(parseA[0], "found") == 0) {
            balance += atoi(parseA[1]);
        } else {
            count += 1;
        }
        if (strcmp(parseB[0], "found") == 0) {
            balance += atoi(parseB[1]);
        } else {
            count += 1;
        }
        if (strcmp(parseC[0], "found") == 0) {
            balance += atoi(parseC[1]);
        } else {
            count += 1;
        }

        // if user not found in all three backend log files: "cw f1"
        if (count == 3) {
            sprintf(send_message, "cw f1");
        // if user is found in at least one backend log files: "cw s balance"
        } else {
            sprintf(send_message, "cw s %d", balance);
        }

        // send this message back to the client:
        if ((numbytes = send(new_fd, send_message, strlen(send_message), 0) == -1)) {
            perror("failed to send message back to client");
        }

        // generate on-screen message after check wallet operation is done on the server M side:
        // if the user is not found, based on Piazza post @164, print "Username was not found on database."
        if (count == 3) {
            printf("Username was not found on database.\n");
        } else {
            printf("The main server sent the current balance to client %s.\n", client);
        }

    // if command was transfer coins:
    } else if (strcmp(parsed_message[0], "txc") == 0) {
        // there will be 4 different cases for information in parseA, parseB, and parseC:
        // 1. [found, found, maxSeq, summation of balance of sender]: this case indicate both sender and receiver are found
        // 2. [notfound, notfound, maxSeq, 0]: both sender and receiver are not found
        // 3. [notfound, found, maxSeq, 0]: sender not found, receiver found
        // 4. [found, notfound, maxSeq, summation of balance of sender]: sender found, receiver not found
        int countSender = 0;                                                     // number of sender not found
        int countReceiver = 0;                                                   // number of receiver not found
        int seq = max(atoi(parseA[2]), max(atoi(parseB[2]), atoi(parseC[2])));   // max seq among all three backend log files, used to know which the seq for next log entry
        int senderBalance = 1000;                                                // initial sender balance is 1000
        
        // count how many times the sender/reciver is not found, and add summation of transaction to sender's balance if sender is found:
        //-------------------------------------------------------------------------------------------------------------------------------
        // result from server A:
        if (strcmp(parseA[0], "found") == 0 && strcmp(parseA[1], "found") == 0) {
            senderBalance += atoi(parseA[3]);
        } else if (strcmp(parseA[0], "notfound") == 0 && strcmp(parseA[1], "notfound") == 0) {
            countSender += 1;
            countReceiver += 1;
        } else if (strcmp(parseA[0], "notfound") == 0 && strcmp(parseA[1], "found") == 0) {
            countSender += 1;
        } else if (strcmp(parseA[0], "found") == 0 && strcmp(parseA[1], "notfound") == 0) {
            senderBalance += atoi(parseA[3]);
            countReceiver += 1;
        }
        // result from server B:
        if (strcmp(parseB[0], "found") == 0 && strcmp(parseB[1], "found") == 0) {
            senderBalance += atoi(parseB[3]);
        } else if (strcmp(parseB[0], "notfound") == 0 && strcmp(parseB[1], "notfound") == 0) {
            countSender += 1;
            countReceiver += 1;
        } else if (strcmp(parseB[0], "notfound") == 0 && strcmp(parseB[1], "found") == 0) {
            countSender += 1;
        } else if (strcmp(parseB[0], "found") == 0 && strcmp(parseB[1], "notfound") == 0) {
            senderBalance += atoi(parseB[3]);
            countReceiver += 1;
        }
        // result from server C:
        if (strcmp(parseC[0], "found") == 0 && strcmp(parseC[1], "found") == 0) {
            senderBalance += atoi(parseC[3]);
        } else if (strcmp(parseC[0], "notfound") == 0 && strcmp(parseC[1], "notfound") == 0) {
            countSender += 1;
            countReceiver += 1;
        } else if (strcmp(parseC[0], "notfound") == 0 && strcmp(parseC[1], "found") == 0) {
            countSender += 1;
        } else if (strcmp(parseC[0], "found") == 0 && strcmp(parseC[1], "notfound") == 0) {
            senderBalance += atoi(parseC[3]);
            countReceiver += 1;
        }
        //-------------------------------------------------------------------------------------------------------------------------------

        // if all three files don't have sender and receiver: "txc f3"
        if (countSender == 3 && countReceiver == 3) {
            sprintf(send_message, "txc f3");
        // sender not found in the network: "txc f2s"
        } else if (countSender == 3) {
            sprintf(send_message, "txc f2s");
        // receiver not found in the network: "txc f2r"
        } else if (countReceiver == 3) {
            sprintf(send_message, "txc f2r");
        // sender has insufficient balance: "txc f1 <sender's current balance>"
        } else if (senderBalance < atoi(parsed_message[3])) {
            sprintf(send_message, "txc f1 %d",senderBalance);
        // sender has sufficient balance, and sender and receiver both found in the network: "txc s <sender's balance after this transaction>"
        } else {
            sprintf(send_message, "txc s %d", (senderBalance - atoi(parsed_message[3])));
            
            // For this case, we need to generate a new log entry:
            char udpMessage[MAXDATASIZE];
            char *udpMessage_parsed[10];
            char udpRecv[MAXDATASIZE];

            // message would be "wlog <new log entry seq number> sender receiver #coins"
            sprintf(udpMessage, "wlog %d %s %s %s", seq + 1, parsed_message[1], parsed_message[2], parsed_message[3]);

            // parse the message: 
            parse_message(udpMessage_parsed, udpMessage);
            
            // randomly select a backend server for log entry:
            if (serverNum == 0) {
                udp_send_recv(udpMessage_parsed, udpRecv, SERVER_A_PORT, sockfd_UDP, serverA);
            } else if (serverNum == 1) {
                udp_send_recv(udpMessage_parsed, udpRecv, SERVER_B_PORT, sockfd_UDP, serverB);
            } else {
                udp_send_recv(udpMessage_parsed, udpRecv, SERVER_C_PORT, sockfd_UDP, serverC);
            }

            // for debug, not really print this message:
            // if (strcmp(udpRecv, "log entry added successfully") == 0) {
            //     printf("Log generated successfully.\n");
            // }
        }

        // after all is done, send feedback back to client:
        if ((numbytes = send(new_fd, send_message, strlen(send_message), 0) == -1)) {
            perror("failed to send message back to client");
        }

        // print the on-screen message to show the transfer coin operation is finished on the server M side:
        // for case where sender/receiver not in the network, the on-screen message changes based on Piazza post @164 and @347:
        if (countSender == 3 || countReceiver == 3) {
            printf("Username was not found on database.\n");
        } else {
            printf("The main server sent the result to the transaction to client %s.\n", client);
        }

    // if the command is TXLIST
    } else if (strcmp(parsed_message[0], "txl") == 0) {
        // generate "alichain.txt"
        write_sorted_list_log(parseA, parseB, parseC);

        // print the on-screen message to show the sorted file is up and ready, base on Piazza post @292, print "The sorted file is up and ready."
        printf("The sorted file is up and ready.\n");
    }
}

int main(void) {
    srand(time(NULL));                       // seed for rand()
    int sockfd_TCP_A, new_fd_A;              // for Client A
    int sockfd_TCP_B, new_fd_B;              // for Client B
    int sockfd_UDP;                          // for backend servers
    struct sockaddr_storage their_addr;      // for accept()
    socklen_t sin_size;                      // for accept()   
    char s[INET6_ADDRSTRLEN];

    // initialize TCP and UDP sockets:
    sockfd_TCP_A = initTCP(TCPPORTA);
    sockfd_TCP_B = initTCP(TCPPORTB);
    sockfd_UDP = initUDP(UDPPORT);

    char clientA[] = "A";
    char clientB[] = "B";

    // server M is up on-screen message:
    printf("The main server is up and running.\n");

    // we assume we always run the client programs with order:  A - B - A - B - A - B ------------
    while (1) {
        int serverNum = rand() % 3;  // this will randomly generate 0 (serverA), 1 (serverB), 2 (serverC)
        sin_size = sizeof their_addr;

        // Deal with Client A request:
        new_fd_A = accept(sockfd_TCP_A, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd_A == -1) {
            perror("client A: accept");
            continue;
        }
        if (!fork()) {
            close(sockfd_TCP_A);
            tcp_client_operation(new_fd_A, sockfd_UDP, TCPPORTA, clientA, serverNum);
            close(new_fd_A);
            exit(0);            
        }
        close(new_fd_A);

        // Deal with Client B request:
        new_fd_B = accept(sockfd_TCP_B, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd_B == -1) {
            perror("client B: accept");
            continue;
        }
        if (!fork()) {
            close(sockfd_TCP_B);
            tcp_client_operation(new_fd_B, sockfd_UDP, TCPPORTB, clientB, serverNum);
            close(new_fd_B);
            exit(0);            
        }
        close(new_fd_B);      
    }
    return 0;
}
