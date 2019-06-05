
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h> 

#define MAXLEN 524
#define MAXSEQ 25600

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

int port;
char* host = NULL;
// char* test = "TESTING TESTING";
char* filename = NULL;







int main(int argc, char* argv[]){
	char buf[512];
	int sock;
	struct sockaddr_in servername;
	// struct hostent *server;

	if (argc != 4){
		fprintf(stderr, "Invalid number of arguments. \n");
		exit(1);
	}

	host = argv[1];
	port = atoi(argv[2]);
	filename = argv[3];

	if(host == NULL){
		host = "localhost";
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0){
		fprintf(stderr, "Creating a socket failed. %s\r\n", strerror(errno));
		exit(1);
	}

	servername.sin_family = AF_INET;
	servername.sin_port = htons(port);
	servername.sin_addr.s_addr = INADDR_ANY;

	int n, len;
	// n = sendto(sock, (const char *)filename, strlen(filename), 0, (const struct sockaddr *) &servername, sizeof(servername));
	// if (n < 0)
	// 	fprintf(stderr, "ERROR in sendto");	

	srand(time(0));
	int seqnum = rand() % (MAXSEQ + 1 - 0) + 0;
	int ack = 0;

	struct udpheader packet_header = {seqnum, ack, 0, 1, 0, 0}; // {seqnum, ack, ack_flag, SYN_flag, FIN_flag, padding}
	// printf("%d\n", sizeof(packet_header));
	// printf("%d\n", seqnum);

	FILE* fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "file not found. \n" );
		exit(1);
	}

	char output[MAXLEN] = {0};
	// int bytes_read = read(fileno(fp), &buf, 512);
	// printf("%s\n", buf);
	// struct packet p = {packet_header};
	// strncpy(p.payload, buf, sizeof(buf));
	// printf("%d\n", p.packet_header.sequence_number);
	// printf("%s\n", p.payload);
	// n = sendto(sock, (struct packet*)&p, (sizeof(p)), 0, (struct sockaddr *) &servername, sizeof(servername));

	// n = sendto(sock, (struct udpheader*)&packet_header, (1024+sizeof(packet_header)), 0, (struct sockaddr *)
 //         &servername, sizeof(servername));

	int bytes_read;
	while((bytes_read = read(fileno(fp), &buf, 512)) != EOF){
		struct packet p = {packet_header};
		strncpy(p.payload, buf, sizeof(buf));
		// printf(sizeof(p));
		void *out = buf;
		while(bytes_read > 0){
			int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &servername, sizeof(servername));
			if(packet_written <= 0){
				fprintf(stderr, "unable to write to socket");
				exit(1);
			}
			int bytes_written = sizeof(buf);
			int n, len;
			n = recvfrom(sock, (char *) buf, MAXLEN, 0, &servername, &len);
			if (n < 0)
				fprintf(stderr, "ERROR in recvfrom");
			buf[n] = '\0';
			printf("Echo from server: %s\n", buf);

			bytes_read -= bytes_written;
			out += bytes_written;
		}
	}

	n = recvfrom(sock, (char *) buf, MAXLEN, 0, &servername, &len);
	if (n < 0)
		fprintf(stderr, "ERROR in recvfrom");
	buf[n] = '\0';

	close(sock);
	// printf("%d\n", n);
	printf("Echo from server: %s\n", buf);
	// printf("Expected: ");
	// printf("%s\n", filename);
	return 0;

}