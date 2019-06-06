
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
#include <regex.h>

#define MAXLEN 524
#define MAXSEQ 25600
//fsefsd
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



int match(const char *string, const char *pattern)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) return 0;
    int status = regexec(&re, string, 0, NULL, 0);
    regfree(&re);
    if (status != 0) return 0;
    return 1;
}




int main(int argc, char* argv[]){
	char buf[512];
	int sock;
	struct sockaddr_in servername;
	// struct hostent *server;


	if (argc != 4){
		fprintf(stderr, "Invalid number of arguments. \n");
		exit(1);
	}



	
	const char* exp = "^[a-zA-Z0-9 ]*$";
	//error checking for hostname
	if (match(argv[1], exp))
	{
		host = argv[1];
	}else{
		printf("Invalid hostname\n");
		exit(1);
	}




	//error checking port number
	const char *s = argv[2];
	char *eptr;
	long value = strtol(s, &eptr, 10); /* 10 is the base */
	/* now, value is 232, eptr points to "B" */
	if (strlen(eptr) == 0)
	{
		port = atoi(argv[2]);
	}else{
		fprintf(stderr, "Invalid port number\n" );
		exit(1);
	}

	//no error checking required
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

	// Initiate threeway handshake
	srand(time(0));
	int seqnum = rand() % (MAXSEQ + 1 - 0) + 0;
	int ack = 0;
	printf("sequence number for packet 1 being sent: %d\n", seqnum);

	struct udpheader packet_header = {seqnum, ack, 0, 1, 0, 0}; // {seqnum, ack, ack_flag, SYN_flag, FIN_flag, padding}
	// printf("%d\n", sizeof(packet_header));
	// printf("%d\n", seqnum);
	struct packet p_syn = {packet_header};
	n = sendto(sock, (struct packet*) &p_syn, sizeof(p_syn), 0, (struct sockaddr *) &servername, sizeof(servername));
	if(n <= 0){
		fprintf(stderr, "ERROR: unable to send initiation packet");
	}

	// receive syn and ack flags in packet from server
	struct packet * r_syn = malloc(sizeof(struct packet));
	n = recvfrom(sock, r_syn, sizeof(*r_syn), 0, (struct sockaddr *) &servername, &len);
	if (n < 0){
		fprintf(stderr, "ERROR: recvfrom");
	}

	// verify ack number from packet from server is seqnum + 1
	if(r_syn->packet_header.ack_number != seqnum+1){
		fprintf(stderr, "ERROR: incorrect ack number from server. Received: %d Expected: %d\n", r_syn->packet_header.ack_number, seqnum+1);
	}

	// printf("hit")
	// verify that SYN flag is up
	if(r_syn->packet_header.SYN != 1){
		fprintf(stderr, "ERROR: SYN flag not up in handshake packet (part 2) received from server.");
	}

	// verify that ACK flag is up
	if(r_syn->packet_header.ACK != 1){
		fprintf(stderr, "ERROR: ACK Flag not up in handshake packet (part 2) received from server.");
	}

	printf("sequence number for packet 1 received: %d\n", r_syn->packet_header.sequence_number);
	printf("ack number for packet 1 received: %d\n", r_syn->packet_header.ack_number);


	FILE* fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "file not found. \n" );
		exit(1);
	}

	// char output[MAXLEN] = {0};
	// int bytes_read = read(fileno(fp), &buf, 512);
	// printf("%s\n", buf);
	// struct packet p = {packet_header};
	// strncpy(p.payload, buf, sizeof(buf));
	// printf("%d\n", p.packet_header.sequence_number);
	// printf("%s\n", p.payload);
	// n = sendto(sock, (struct packet*)&p, (sizeof(p)), 0, (struct sockaddr *) &servername, sizeof(servername));

	// n = sendto(sock, (struct udpheader*)&packet_header, (1024+sizeof(packet_header)), 0, (struct sockaddr *)
 //         &servername, sizeof(servername));

	// third part of handshake
	seqnum = r_syn->packet_header.ack_number;
	ack = r_syn->packet_header.sequence_number+1;
	char ack_flag = 1;
	char syn_flag = 0;
	char fin_flag = 0;

	// int bytes_read = read(fileno(fp), &buf, 512);
	// printf("%s\n", buf);
	// struct udpheader p_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0};
	// struct packet p = {p_header};
	// strncpy(p.payload, buf, sizeof(buf));
	// printf("sequence number for packet 2 being sent: %d\n", p.packet_header.sequence_number);
	// // printf("%s\n", p.payload);
	// n = sendto(sock, (struct packet*)&p, (sizeof(p)), 0, (struct sockaddr *) &servername, sizeof(servername));

	// struct packet * r = malloc(sizeof(struct packet));
	// n = recvfrom(sock, r, sizeof(*r), 0, (struct sockaddr *) &servername, &len);
	// if (n < 0)
	// 	fprintf(stderr, "ERROR in recvfrom");

	// seqnum = r->packet_header.sequence_number;
	// ack = 0;
	// ack_flag = 0;

	// printf("sequence number for packet 2 received: %d\n", seqnum);
	// printf("ack number for packet 2 refceived: %d\n", r->packet_header.ack_number);

	// // struct udpheader packet_header = {r_syn->packet_header.ack_number, r_syn.packet_header.sequence_number+1, 1, 0, 0, 0};

	char rep_buf[512] = {0};

	int bytes_read;
	while((bytes_read = read(fileno(fp), &buf, 512)) != EOF){
		if(bytes_read == 0){
			break;
		}
		printf("bytes read: %d\n", bytes_read);
		if(bytes_read < 512){
			buf[bytes_read] = '\0';
		}
		// printf("%s\n", buf);
		struct udpheader p_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0};
		struct packet p = {p_header};
		strncpy(p.payload, buf, sizeof(buf));
		// printf("%d\n", sizeof(buf));
		printf("sequence number for packet 2 being sent: %d\n", p.packet_header.sequence_number);
		// printf(sizeof(p));
		void *out = buf;
		while(bytes_read > 0){
			int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &servername, sizeof(servername));
			if(packet_written <= 0){
				fprintf(stderr, "unable to write to socket");
				exit(1);
			}
			int bytes_written = sizeof(buf);
			// int n, len;
			struct packet * r = malloc(sizeof(struct packet));
			n = recvfrom(sock, r, sizeof(*r), 0, (struct sockaddr *) &servername, &len);
			if (n < 0){
				fprintf(stderr, "ERROR in recvfrom\n");
				exit(1);
			}

			seqnum = r->packet_header.ack_number;
			ack = 0;
			ack_flag = 0;
			printf("sequence number for packet 2 received: %d\n", seqnum);
			printf("ack number for packet 2 received: %d\n", r->packet_header.ack_number);
			// buf[n] = '\0';
			// printf("Echo from server: %s\n", buf);

			bytes_read -= bytes_written;
			out += bytes_written;
		}
	}

	// n = recvfrom(sock, (char *) buf, MAXLEN, 0, &servername, &len);
	// if (n < 0)
	// 	fprintf(stderr, "ERROR in recvfrom");
	// buf[n] = '\0';

	close(sock);
	// printf("%d\n", n);
	// printf("Echo from server: %s\n", buf);
	// printf("Expected: ");
	// printf("%s\n", filename);
	return 0;

}