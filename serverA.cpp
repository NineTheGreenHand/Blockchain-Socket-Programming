// serverA.cpp
// Back-ServerA file for the socket programming project

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
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <sstream>

using namespace std;

#define HOSTADDRESS "127.0.0.1" // Local host address
#define APORT "21687"           // UDP port number used by ServerA
#define MPORT "24687"           // UDP port number used by ServerM
#define LOGFILE "block1.txt"    // File that is accessible by ServerA
#define MAXDATASIZE 10000       // Max data size in byte

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

// get the number of log entries from the server log:
int get_log_nums() {
    ifstream file(LOGFILE);
    int logNum = 0;
    string line;
    while (getline(file, line)) {
        logNum += 1;
    }
    file.close();
    return logNum;
}

// read the log file, and locate all transactions related to the target user
// return values contains 3 int values:
// [max seq number, 0 (if user is not found) / 1 (if found), summation of money transfer in & out]
vector<int> read_File(char *targetUser) {
    vector<int> seq_present_balance;
    string user;
    user += targetUser;
    int seq = 0, balance = 0, present = 0;
    ifstream file(LOGFILE);
    string line;
    while (getline(file, line)) {
        vector<string> words;       // words: [seq number, sender, receiver, coins];
        stringstream ss(line);
        string word;
        while (getline(ss, word, ' ')) {
            words.push_back(word);
        }
        
        char seqA[words[0].length() + 1];
        strcpy(seqA, words[0].c_str());
        char coinA[words[3].length() + 1];
        strcpy(coinA, words[3].c_str());
    
        seq = max(seq, atoi(seqA));

        if (words[1] == user) {
            balance -= atoi(coinA);
            present = 1;
        } else if (words[2] == user) {
            balance += atoi(coinA);
            present = 1;
        }
    }
    file.close();
    seq_present_balance.push_back(seq);
    seq_present_balance.push_back(present);
    seq_present_balance.push_back(balance);
    return seq_present_balance;
}

// analyze the message got from serverM, then execute that command
// and generate the result and return that result:
const char* execute_command(char *message[]) {
    static char temp[MAXDATASIZE];
    // if the command was check wallet:
    if (strcmp(message[0], "cw") == 0) {
        vector<int> read_file_result = read_File(message[1]);
        if (read_file_result[1] == 1) {
            sprintf(temp, "found %d", read_file_result[2]);
        } else {
            sprintf(temp, "notfound");
        }
    // if the command was transfer coins:
    } else if (strcmp(message[0], "txc") == 0) {
        vector<int> sender = read_File(message[1]);
        vector<int> receiver = read_File(message[2]);
        if (sender[1] == 0 && receiver[1] == 0) {
            sprintf(temp, "notfound notfound %d 0", sender[0]);
        } else if (sender[1] == 1 && receiver[1] == 1) {
            sprintf(temp, "found found %d %d", sender[0], sender[2]);
        } else if (sender[1] == 1 && receiver[1] == 0) {
            sprintf(temp, "found notfound %d %d", sender[0], sender[2]);
        } else if (sender[1] == 0 && receiver[1] == 1) {
            sprintf(temp, "notfound found %d 0", sender[0]);
        }
    // if the command was TXLIST:
    } else if (strcmp(message[0], "txl") == 0) {
        int logNum = get_log_nums();
        ifstream file(LOGFILE);
        string line;
        sprintf(temp, "%d", logNum);
        while (getline(file, line)) {
            sprintf(temp, "%s\n%s", temp, line.c_str());
        }
        file.close();
    }
    return temp;
}

// write the transaction log into the log file:
void write_log(char *log[]) {
    ofstream file;
    file.open(LOGFILE, ios::app);
    string line;
    for (int i = 1; i <= 4; i++) {
        line = line + log[i] + " ";
    }
    file << "\n";
    file << line;
    file.close();
}

int main(void) {
    // UDP Socket Intialization:
    int sockfd, rv, numbytes;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char buf[MAXDATASIZE];
    char s[INET6_ADDRSTRLEN];
    addr_len = sizeof(their_addr);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;    // to use IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // type is UDP

    // Get backend server information, we use HOSTADDRESS and APORT in here:
    if ((rv = getaddrinfo(HOSTADDRESS, APORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("ServerA: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("ServerA: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "ServerA: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);
    
    // Print the message to show the server is up:
    printf("The ServerA is up and running using UDP on port %s.\n", APORT);

    while (1) {
        // receive message from server M:
        if ((numbytes = recvfrom(sockfd, buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        // this is the message we get:
        buf[numbytes] = '\0';
        
        // parse the message:
        char *message[10];
        char result[MAXDATASIZE];
        parse_message(message, buf);

        // internal communications do not generate extra on screen message for writing log entry operation
        // for the transfer coin command, if server A is chosen, it will receive the "wlog" command which means
        // write the new log entry to its log file:
        if (strcmp(message[0], "wlog") == 0) {
            write_log(message);
            char feedback[MAXDATASIZE];     // message we sent back to server M (not really used, just as a message to send back)
            sprintf(feedback, "log entry added successfully");
            if ((numbytes = sendto(sockfd, feedback, strlen(feedback), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
                perror("sendto");
                exit(1);
            }
        // for other operations, it will print the on-screen messages:
        } else {
            printf("The ServerA received a request from the Main Server.\n");
            strcpy(result, execute_command(message));
            if ((numbytes = sendto(sockfd, result, strlen(result), 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
                perror("sendto");
                exit(1);
            }
            printf("The ServerA finished sending the response to the Main Server.\n");
        }       
    }
    return 0;
}
