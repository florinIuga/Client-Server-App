#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include <cstring>
#include <iostream>
#include <vector>
#define LEN 50
using namespace std;


struct __attribute__((packed)) Topic {

	char topicName[50];
	int SF; // 0 sau 1
	bool isStillSubscribed;
};

struct __attribute__((packed)) UDPMessage {

	char topic[50];
	char IP[20];
	unsigned char type;
	int power;
	int port;
	int intVal; // for type = 0
	float shortVal; // for type = 1
	float floatVal; // fort type = 2
	char message[1500]; // for type = 3
	bool sentSFMessage;
};

struct ClientTCP {

	char IP[20];
	char ID[10];
	int sockfd; // pentru a sti pe ce socket sa trimit
	std::vector<Topic> subscribedTopics; // topicurile la care a dat subscribe clientul
	std::vector<UDPMessage> sfMessages; // mesajele care trebuie trimise clientului daca are SF = 1 cat timp a fost delogat
	bool isOffline;
};

struct __attribute__((packed)) TCPMessage {

	char subscrMsg[15];
	char topic[50];
	int SF;
};

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buff[50];

	fd_set read_fds, tmp_fds; 

	if (argc < 3) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	// send message with client ID
	char ID[10];
	strcpy(ID, argv[1]);
	n = send(sockfd, ID, strlen(ID), 0);
	DIE(n < 0, "error sending ID from TCP to server\n");

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	char delim[] = " ";

	while (1) {
  		// se citeste de la tastatura

  		tmp_fds = read_fds; // restore fds
  		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL); // multiplexam cu select

  		// de aici in jos incep verificarile pe file descriptori

  	  if (FD_ISSET(STDIN_FILENO, &tmp_fds)) { // daca am primit ceva dde la stdin
  	  	
  	  		memset(buff, 0, LEN);
			fgets(buff, LEN - 1, stdin);

			TCPMessage newTCPMsg;

		if (strcmp(buff, "exit\n") == 0) {
				break;

		} else {

				char *token = strtok(buff, delim);

					if (strcmp(token, "subscribe") == 0) {

						strcpy(newTCPMsg.subscrMsg, token);
						token = strtok(NULL, delim);
						if (token == NULL) {
							cout << "You must enter <subscribe> <topic> <SF>\n";
						} else {
							strcpy(newTCPMsg.topic, token);
							token = strtok(NULL, delim);
							if (token == NULL) {
								cout << "You must enter <subscribe> <topic> <SF>\n";
							} else {
								newTCPMsg.SF = atoi(token);
							}
						}
						
					} else if (strcmp(token, "unsubscribe") == 0) {

						strcpy(newTCPMsg.subscrMsg, token);
						token = strtok(NULL, delim);
						strcpy(newTCPMsg.topic, token);
					
					} else {

						cout << "Please enter a valid command for client. \n";
					}
				}	
			// se trimite mesaj la server
			n = send(sockfd, (char*) &newTCPMsg, sizeof(newTCPMsg), 0);
			DIE(n < 0, "send from TCP to SERVER");
		}

		if (FD_ISSET(sockfd, &tmp_fds)) { // daca am primit ceva de la server (de pe socketul sockfd)
 			
 			UDPMessage udpMsg;

 			// primeste mesajul de la publisher, adica de la UDP
            n = recv(sockfd, (char*) &udpMsg, sizeof(udpMsg), 0);
            DIE(n < 0, "error recv from SERVER TO TCP");
            
            int t = udpMsg.type;
            	
            	// case for INT
            	if (t == 0) {
					cout << udpMsg.IP << ":" << udpMsg.port << " - " << udpMsg.topic << " - INT - " << udpMsg.intVal << "\n";

            	} else if (t == 1) {
					cout << udpMsg.IP << ":" << udpMsg.port << " - " << udpMsg.topic << " - SHORT_REAL - ";
					printf("%.2f\n", udpMsg.shortVal);

            	} else if (t == 2) {
            		cout << udpMsg.IP << ":" << udpMsg.port << " - " << udpMsg.topic << " - FLOAT - ";
            		printf("%.*f\n", udpMsg.power, udpMsg.floatVal);

            	} else if (t == 3) {
            		cout << udpMsg.IP << ":" << udpMsg.port << " - " << udpMsg.topic << " - STRING - " << udpMsg.message << "\n";

            		// if type == 4 it means it's exit
            	} else if (t == 4) {
            		
            		break;

            	} else {
            		cout << "The udp message was not received properly. \n";
            	}
			}
	}

	close(sockfd);

	return 0;
}
