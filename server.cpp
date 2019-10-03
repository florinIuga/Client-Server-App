#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include "helpersUDP.h"
#include <vector>
#include <cmath>
#include <string.h>
#include <stdlib.h>
#include <iostream>

using namespace std;

struct __attribute__((packed)) Topic {

	char topicName[50];
	int SF;
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

struct __attribute__((packed)) TCPMessage {

	char subscrMsg[15];
	char topic[50];
	int SF;
};

struct ClientTCP {

public:

	char IP[20];
	char ID[10];
	int sockfd;
	std::vector<Topic> subscribedTopics; // topicurile la care a dat subscribe clientul
	std::vector<UDPMessage> sfMessages; // mesajele care trebuie trimise clientului daca are SF = 1 cat timp a fost delogat
	bool isOffline;
};

bool isSubscribed(ClientTCP client, char topic[]) {

	for (unsigned int i = 0; i < client.subscribedTopics.size(); ++i) {
		if (strcmp(client.subscribedTopics[i].topicName, topic) == 0 && client.subscribedTopics[i].isStillSubscribed == true) {
			return true;
		}
	}
	return false;
}

bool clientSFIsOnForTopic(ClientTCP client, char topic[]) {

	for (unsigned int i = 0; i < client.subscribedTopics.size(); ++i) {
		if (strcmp(client.subscribedTopics[i].topicName, topic) == 0 && client.subscribedTopics[i].SF == 1) {
			return true;
		}
	}
	return false;
}

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

bool clientAlreadyExists(vector<ClientTCP> &clients, char ID[]) {
	for (unsigned int i = 0; i < clients.size(); ++i) {
		if (strcmp(clients[i].ID, ID) == 0) {
			return true;
		}
	}
	return false;
}

int existsTopic(ClientTCP c, char topic[]) {
	for (unsigned int k = 0; k < c.subscribedTopics.size(); ++k) {
		if (strcmp(c.subscribedTopics[k].topicName, topic) == 0) {
			return k;
		}
	}
	return -1;
}

int main(int argc, char *argv[])
{
	int sockfdTCP, newsockfdTCP, portno;
	int sockfdUDP;

	struct sockaddr_in serv_addr, cli_addr;
	int n, i, retTCP, retUDP;
	socklen_t clilen;
	
	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// open the TCP socket
	sockfdTCP = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfdTCP < 0, "socket TCP");
	
	// open the the UDP socket
	sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockfdUDP < 0, "socket UDP");

	// port
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");
	
	// set serv_addr struct for telling where to send the data 
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	
	// link the TCP socket properties
	retTCP = bind(sockfdTCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(retTCP < 0, "bind TCP");

	// link the UDP socket properties
	retUDP= bind(sockfdUDP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(retUDP < 0, "bind UDP");
	
	// listen to clients TCP and UDP
	retTCP = listen(sockfdTCP, MAX_CLIENTS);
	DIE(retTCP < 0, "listen TCP");

	// add sockfdTCP and sockfdUDP to the set
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfdTCP, &read_fds);
	FD_SET(sockfdUDP, &read_fds);
	
	fdmax = max(sockfdTCP, sockfdUDP);
	int ret;

	char bufUdp[1551];
	int udpSize = 1551;

	struct sockaddr_in from_UDP;
	vector<ClientTCP> clients;

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL); 

		DIE(ret < 0, "select");
		
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfdTCP) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfdTCP = accept(sockfdTCP, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfdTCP < 0, "accept");
					char clientID[10];
					// primeste ID-ul clientului TCP
					memset(clientID, 0, sizeof(clientID));
					n = recv(newsockfdTCP, clientID, sizeof(clientID), 0);
					DIE(n < 0, "error receiving client TCP id\n");
					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfdTCP, &read_fds);

					if (newsockfdTCP > fdmax) { 
						fdmax = newsockfdTCP;
					}
					cout << "New client " << clientID << " connected from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << ".\n";
					// if the newClient was subscribed to a topic and his SF for that topic was 1, send the messages he has lost
					int verify;
					if (clientAlreadyExists(clients, clientID)) {
						// cout << "da\n";
						for (unsigned int j = 0; j < clients.size(); ++j) {

							if (strcmp(clients[j].ID, clientID) == 0) {
								// daca clientul a mai fost o data conectat ii pastrez toate datele si ii updatez doar sockfd-ul
								clients[j].sockfd = newsockfdTCP;
								clients[j].isOffline = false;
							
								for (unsigned int k = 0; k < clients[j].sfMessages.size(); ++k) {
									
									if (clients[j].sfMessages[k].sentSFMessage == false) {
										verify = send(clients[j].sockfd, (char*) &clients[j].sfMessages[k], sizeof(clients[j].sfMessages[k]), 0);
										clients[j].sfMessages[k].sentSFMessage = true;
										DIE(verify < 0, "send to TCP sfMessages from server");
									}
								}
								break; // stop iterating because we found the client
							}
						}

						// else, if the client hadn't previously been connected, create a new client and add it to the list of clients
					} else {
						
						ClientTCP newClient;
						newClient.sockfd = newsockfdTCP; 
						newClient.isOffline = false;
						memcpy(newClient.ID, clientID, sizeof(clientID));
						clients.push_back(newClient);
					}
					
				// a venit o cerere pe socket UDP
				} else if (i == sockfdUDP) {

					char topic[50]; // topic name
					uint8_t type; // can be: 0 1 2 3

					uint32_t intVal; // for type = 0
					uint16_t shortVal; // for type = 1
					uint32_t floatVal; // for type = 2
					char message[1500]; // for type = 3

					char UdpIP[20];
					int portUdp;

					socklen_t fromlen;
					
					memset(bufUdp, 0, 1551);
					ret = recvfrom(sockfdUDP, bufUdp, udpSize, 0, (struct sockaddr*) &from_UDP, &fromlen);
					
					memcpy(UdpIP, inet_ntoa(from_UDP.sin_addr), 20);
					
					portUdp = ntohs(from_UDP.sin_port);
					
					memcpy(topic, bufUdp, sizeof(topic));
					memcpy(&type, bufUdp + 50, sizeof(uint8_t));

					int t = type;
					uint8_t power;
					uint8_t sign; // pentru cazul type = 0

					UDPMessage msg;
					
					memcpy(msg.topic, topic, sizeof(topic));
					memcpy(msg.IP, UdpIP, sizeof(UdpIP));
					msg.port = portUdp;
					

					switch (t) {

						case 0:
							  memcpy(&sign, bufUdp + 50 + 1, sizeof(uint8_t));
							  memcpy(&intVal, bufUdp + 50 + 1 + sizeof(uint8_t), sizeof(uint32_t));
							  
							  msg.type = 0;
							  // positive number
							  if (sign == 0) {
							  	msg.intVal = ntohl(intVal);
							  	
							  } else {
							  	msg.intVal = -ntohl(intVal);
							  }
							  break;

						case 1:
							  memcpy(&shortVal, bufUdp + 50 + 1, sizeof(uint16_t));
							  msg.type = 1;
							  msg.shortVal = (float) (ntohs(shortVal) / 100.0f);
							  break;

						case 2:
							{
							  memcpy(&sign, bufUdp + 50 + 1, sizeof(uint8_t));
							  memcpy(&floatVal, bufUdp + 50 + 1 + sizeof(uint8_t), sizeof(uint32_t));
							  memcpy(&power, bufUdp + 50 + 1 + sizeof(uint8_t) + sizeof(uint32_t), sizeof(uint8_t));
							  int p = power;
							  msg.type = 2;
							  msg.power = p;
							  if (sign == 0) {
							  	  msg.floatVal = (float) (ntohl(floatVal) / pow(10, p));
							  } else {
							  	  msg.floatVal = -(float) (ntohl(floatVal) / pow(10, p));
							  }
							}
							  break;

						case 3: // inseamna ca e sir de caractere

							  msg.type = 3;
							  memset(message, 0, sizeof(message));
							  memcpy(message, bufUdp + 50 + 1, sizeof(message)); // + 1 for the type
							  memset(msg.message, 0, 1500);
							  memcpy(msg.message, message, strlen(message)); // ??

							  break;

					    default:
					    	   cout << "Error parsing UDP message\n";

						}

						int check;
						// now send the messages to the Tcp clients who subscribed the current topic
						
						for (unsigned int j = 0; j < clients.size(); ++j) {
							
							if (isSubscribed(clients[j], topic) && !clients[j].isOffline) {
								
								check = send(clients[j].sockfd, (char*) &msg, sizeof(msg), 0);
								DIE(check < 0, "send to TCP from UDP");
								// and save the messages for offline subscribers who want to receive messages regarding that topic (sf = 1)
							} else if (isSubscribed(clients[j], topic) && clients[j].isOffline) {

								 if (clientSFIsOnForTopic(clients[j], topic)) {
								 	
								 	 msg.sentSFMessage = false;
								 	 clients[j].sfMessages.push_back(msg);
								 } 
							}
						}

					DIE(ret < 0, "Error");

				} else if (i == STDIN_FILENO) { // daca a primit comanda de exit, trebuie sa inchida toate conexiunile

					char buf[10];
					fgets(buf, 10, stdin);
					
					UDPMessage msgExit;
					msgExit.type = 4; // 4 for exit
					
					if (strncmp(buf, "exit", 4) == 0) {
						
						for (unsigned int j = 0; j < clients.size(); ++j) {
							if (clients[j].isOffline == false) {
								n = send(clients[j].sockfd, (char*) &msgExit, sizeof(msgExit), 0);
								DIE(n < 0, "send to TCP from Server exit message");
							}
						}
						close(sockfdTCP);
						close(sockfdUDP);
						return 0;
						
					} else {
						cout << "Invalid command. Din you mean exit?\n";
					}
					
					
				} else {
					// s-au primit date pe unul din socketii de client,
					
					TCPMessage msgTcp;
					Topic t;

					n = recv(i, (char*) &msgTcp, sizeof(msgTcp), 0);
					int pos;

				    if (strcmp(msgTcp.subscrMsg, "subscribe") == 0) {	
						
						memcpy(t.topicName, msgTcp.topic, sizeof(msgTcp.topic));
						t.SF = msgTcp.SF;
						t.isStillSubscribed = true;
						// adauga topicul la care a dat subscribe clientul cu socketul i in lista sa de topicuri
						for (unsigned int j = 0; j < clients.size(); j++) {
							if (clients[j].sockfd == i) {
							   
								pos = existsTopic(clients[j], t.topicName);

								// if pos == 0, it means that the client hadnt been subscribed before to the topic
								if (pos == -1) {
									clients[j].subscribedTopics.push_back(t);
								} else {
									clients[j].subscribedTopics[pos].isStillSubscribed = true;
								}
								
								break;
							} 
						}

					} else if (strcmp(msgTcp.subscrMsg, "unsubscribe") == 0) {
						
						for (unsigned int j = 0; j < clients.size(); ++j) {
							if (clients[j].sockfd == i) {
								
								for (unsigned int k = 0; k < clients[j].subscribedTopics.size(); ++k) {
									
									if (strncmp(clients[j].subscribedTopics[k].topicName, msgTcp.topic, strlen(clients[j].subscribedTopics[k].topicName)) == 0) {
										
										// pentru a evita trimiterea mesajelor catre client daca nu mai este abonat
										clients[j].subscribedTopics[k].isStillSubscribed = false;
										break;
									}
								}
								break;
							}
						}
					} else {
						cout << "Invalid subscribe message from tcp client.\n";
					}

					if (n == 0) {
						// daca conexiunea s-a inchis
						for (unsigned int j = 0; j < clients.size(); ++j) {
							if (clients[j].sockfd == i){
								cout << "Client " << clients[j].ID << " disconnected.\n";
								clients[j].isOffline = true;
								break;
							}
						}
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
						continue; // pt ca nu mai are rost sa trec mai jos, iterez in continuare
					}

					DIE(n < 0, "recv");
					
				}
			}
		}
	}

	close(sockfdTCP);
	close(sockfdUDP);

	return 0;
}
