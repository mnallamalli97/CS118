#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <time.h> 

#define MAX_LENGTH 8332
#define FILE_PATH_SIZE 10000
#define MAXSEQ 25600
//CITE: https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpserver.c

int port;
FILE* fp = NULL;
char *addr;
int sock;

ssize_t newsock;
char input[MAX_LENGTH] = {0};
char output[MAX_LENGTH] = {0};
int addrlen = 0;
int clientlen = 0;
char buf[MAX_LENGTH] = {0};
struct hostent *hostp;
char *hostaddrp; 
struct sockaddr_in serveraddr;
struct sockaddr_in clientaddr;

struct udpheader {
	int sequence_number;
	int ack_number; 
	char ACK;
	char SYN;
	char FIN;
	char pad;
};

struct packet {
	struct udpheader packet_header;
	char payload[512];
};

int make_socket(){
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0){
		fprintf(stderr, "Error making socket. %s\r\n", strerror(errno));
		exit(1);
	}

	int option = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void*) &option, sizeof(int)) < 0){
    	fprintf(stderr, "setsockopt(SO_REUSEADDR) failed. %s\n", strerror(errno));
    	exit(1);
	}
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr))){
		fprintf(stderr, "Unable to bind socket. %s\r\n", strerror(errno));
		exit(1);
	}
	return sock;
}






int main(int argc, char* argv[]){
	
	if (argc < 2){
		fprintf(stderr, "invalid arguments. Must be port and file. %s\r\n", strerror(errno));
	}

	port = atoi(argv[1]);
	sock = make_socket();

	clientlen = sizeof(clientaddr);
	int num = 0;

	struct udpheader * rec = malloc(sizeof(struct udpheader));
	struct packet * prec = malloc(sizeof(struct packet));

	//generate random sequence number
	srand(time(0));

	//default settings for udpheader
	int seqnum = 0;
	int ack = 0;
	char ack_flag = 1;
	char syn_flag = 0;
	char fin_flag = 0;

	while(1){
		//every iteration, zero out the buffer
		char buf[MAX_LENGTH] = {0};

		//will also read from client
		// newsock = recvfrom(sock, buf, MAX_LENGTH, 0,
  //                (struct sockaddr *) &clientaddr, &clientlen);

		// newsock = recvfrom(sock, rec, sizeof(*rec), 0,
  //                (struct sockaddr *) &clientaddr, &clientlen);

		newsock = recvfrom(sock, prec, sizeof(*prec), 0,
                 (struct sockaddr *) &clientaddr, &clientlen);

		// set up udp header for response
		if(prec->packet_header.SYN == 1){
			seqnum = rand() % (MAXSEQ + 1 - 0) + 0;
			syn_flag = 1;
			ack = prec->packet_header.sequence_number+1;
			printf("received syn\n");
		}
		else{
			// printf("payload success\n");
			printf("size of payload: %d\n", strlen(prec->payload));
			ack = prec->packet_header.sequence_number + strlen(prec->payload);
		}

		// struct udpheader rec_header = prec->packet_header;

		// printf("recieved payload: %s\n", prec->payload);
		printf("received sequence_number: %d\n", prec->packet_header.sequence_number);

		if(newsock > 0){
			num++;
			// printf("my new num value is: %d\n", num);
		}

		if(newsock < 0){
			fprintf(stderr, "Socket recvfrom failed. %s\r\n", strerror(errno));
			exit(1);
		}

		//now need to gethostbyaddr
//        hostp = gethostbyaddr((const char *) &clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
//
//        if(hostp < 0 || hostp == NULL){
//            fprintf(stderr, "Error on gethostbyaddr. %s\r\n", strerror(errno));
//            exit(1);
//        }
//
//        hostaddrp = inet_ntoa(clientaddr.sin_addr);
//        if (hostaddrp < 0 || hostaddrp == NULL)
//        {
//            fprintf(stderr, "Error on inet_ntoa \n");
//            exit(1);
//        }

		// fprintf(stdout, "server recieved datagram from %s (%s) \n", hostp->h_name, hostaddrp);
		// fprintf(stdout, "server recieved %d/%d bytes: %s \n", strlen(buf), newsock, buf );
		// fprintf(stderr, "buff: %s\n",  buf);
		//send back to the client
		// int ack = seqnum + 
		printf("seqnumber sent: %d\n", seqnum);
		printf("ack sent: %d\n", ack);
		struct udpheader packet_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0};
		struct packet p = {packet_header};
		int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
		if(packet_written <= 0){
			fprintf(stderr, "unable to write to socket");
			exit(1);
		}
		if(prec->packet_header.SYN == 1){
			seqnum++;
			syn_flag = 0;
		}
		// memset(&buf[0], 0, sizeof(buf));
		// snprintf(buf, sizeof(buf), "%d",num);
		// printf("%s\n", buf);
		// newsock = sendto(sock, (const char *) buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
		// if (newsock < 0)
		// {
		// 	fprintf(stderr, "Error in the sendto function\n" );
		// 	exit(1);
		// }
		// printf("%d\n", newsock);
	}

}
