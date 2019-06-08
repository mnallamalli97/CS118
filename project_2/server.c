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
	short sequence_number;
	int ack_number; 
	char ACK;
	char SYN;
	char FIN;
	char pad;
	short size;
} __attribute__((__packed__));;

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

// char* concat(const char *s1, const char *s2)
// {
//     char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
//     // in real code you would check for errors in malloc here
//     strcpy(result, s1);
//     strcat(result, s2);
//     return result;
// }

int main(int argc, char* argv[]){
	
	if (argc < 2){
		fprintf(stderr, "invalid arguments. Must be port and file. %s\r\n", strerror(errno));
	}

	port = atoi(argv[1]);
	sock = make_socket();

	clientlen = sizeof(clientaddr);

	// struct udpheader * rec = malloc(sizeof(struct udpheader));
	struct packet * prec = malloc(sizeof(struct packet));

	//generate random sequence number
	srand(time(0));

	//default settings for udpheader
	short seqnum = 0;
	int ack = 0;
	char ack_flag = 1;
	char syn_flag = 0;
	char fin_flag = 0;

	// make sure not to process duplicate packets
	int last_seq_rec = -1;

	int filecounter = 0;

	FILE * fp = NULL;

	int sleepcounter = 0;
	int dup_ack = 0;

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

		printf("bytes read: %d\n", newsock);
		// set up udp header for response and process packet
		if (prec->packet_header.sequence_number != last_seq_rec){
			if(prec->packet_header.SYN == 1){
				seqnum = rand() % (MAXSEQ + 1 - 0) + 0;
				syn_flag = 1;
				ack_flag = 1;
				ack = prec->packet_header.sequence_number+1;
				printf("received syn\n");
				//open file to write to
				filecounter++;
				char file_path[512];
				snprintf(file_path, sizeof(file_path), "./%d.file\0", filecounter);
				fp = fopen(file_path, "w");
				if(fp == NULL){
					printf("ERROR: unable to create file\n");
					exit(1);
				}
				printf("creating new file at %s\n", file_path);
			}
			else if(prec->packet_header.FIN == 1){
				ack = prec->packet_header.sequence_number+1;
				ack_flag = 1;
				printf("received fin\n");

				//close file written to
				fclose(fp);
			}
			else{
				// printf("payload success\n");
				printf("size of payload: %d\n", sizeof(prec->payload));
				// printf("payload contains: %s\n", prec->payload);
				// ack = prec->packet_header.sequence_number + sizeof(prec->payload);
				// if(ack = prec->packet_header.sequence_number){
				// 	ack = prec->packet_header.sequence_number + prec->packet_header.size;	
				// }
				ack = prec->packet_header.sequence_number + prec->packet_header.size;
				// int x_size = 512-strlen(prec->payload);
				// char x[x_size] = {0};

				// printf("size of concated payload: %d\n", sizeof(concat(prec->payload, x)));
				//write payload to the file
				// fputs(prec->payload, fp);
				fwrite(prec->payload, 1, prec->packet_header.size, fp);
				// fputs(concat(prec->payload, x), fp);
			}
			printf("received sequence_number: %d\n", prec->packet_header.sequence_number);
			last_seq_rec = prec->packet_header.sequence_number;

			if(newsock < 0){
				fprintf(stderr, "Socket recvfrom failed. %s\r\n", strerror(errno));
				exit(1);
			}
			// int seconds = 0;
			// clock_t before = clock();
			// while(1){
			// 	clock_t difference = clock() - before;
			// 	seconds = difference / CLOCKS_PER_SEC;
			// 	if(difference > 10){
			// 		break;
			// 	}
			// }
			printf("seqnumber sent: %d\n", seqnum);
			printf("ack sent: %d\n", ack);
			struct udpheader packet_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, 0};
			struct packet p = {packet_header};

			// PURELY FOR TESTING, MAKE SURE TO REMOVE //

			// if(sleepcounter == 30){
			// 	sleep(12);
			// 	// sleepcounter = 0;
			// 	sleepcounter++;
			// }
			// else if(sleepcounter % 5 == 0){
			// 	printf("sleepcounter: %d\n", sleepcounter);
			// 	sleepcounter++;
			// 	sleep(2);
			// }
			// else{
			// 	sleepcounter++;
			// 	printf("incrementing sleepcounter to: %d\n", sleepcounter);
			// }

			// REMOVE ABOVE PART - PURELY FOR TESTING

			int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
			if(packet_written <= 0){
				fprintf(stderr, "unable to write to socket");
				exit(1);
			}

			// put syn flag down
			if(prec->packet_header.SYN == 1){
				seqnum++;
				syn_flag = 0;
			}
			if(prec->packet_header.FIN == 1){
				printf("into fin sender\n");
				int rec_ack = 0;
				while(1){
					ack = 0;
					ack_flag = 0;
					fin_flag = 1;
					struct udpheader fin_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, 0};
					struct packet fin_pack = {fin_header};
					int n = sendto(sock, (struct packet*) &fin_pack, sizeof(fin_pack), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
					if(n < 0){
						fprintf(stderr, "ERROR: unable to send FIN from server\n");
					}
					printf("fin sent\n");
					struct packet *ack_pack = malloc(sizeof(struct packet));
					rec_ack = recvfrom(sock, ack_pack, sizeof(*ack_pack), 0, (struct sockaddr *) &clientaddr, &clientlen);
					if(rec_ack < 0){
						fprintf(stderr, "ERROR: negative bytes in fin_ack receive\n");
					}
					// printf("%d\n", rec_ack);
					if(rec_ack > 0){
						printf("ack received\n");
						break;
					}
				}
				// exit(1);
			}

		}
		else{
			printf("received duplicate packet, did not process. Sequence number: %d\n", last_seq_rec);
			dup_ack = prec->packet_header.sequence_number + prec->packet_header.size;
			printf("seqnumber sent: %d\n", seqnum);
			printf("ack sent: %d\n", ack);
			struct udpheader packet_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, 0};
			struct packet p = {packet_header};
			int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
			if(packet_written <= 0){
				fprintf(stderr, "unable to write to socket, dup");
				exit(1);
			}

		}
		// struct udpheader rec_header = prec->packet_header;

		// printf("recieved payload: %s\n", prec->payload);

		// if(newsock < 0){
		// 	fprintf(stderr, "Socket recvfrom failed. %s\r\n", strerror(errno));
		// 	exit(1);
		// }

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
		// int seconds = 0;
		// clock_t before = clock();
		// while(1){
		// 	clock_t difference = clock() - before;
		// 	seconds = difference / CLOCKS_PER_SEC;
		// 	if(difference > 10){
		// 		break;
		// 	}
		// 	sleep(1);
		// }
		// printf("seqnumber sent: %d\n", seqnum);
		// printf("ack sent: %d\n", ack);
		// struct udpheader packet_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0};
		// struct packet p = {packet_header};
		// int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &clientaddr, sizeof(clientaddr));
		// if(packet_written <= 0){
		// 	fprintf(stderr, "unable to write to socket");
		// 	exit(1);
		// }
		// if(prec->packet_header.SYN == 1){
		// 	seqnum++;
		// 	syn_flag = 0;
		// }
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
