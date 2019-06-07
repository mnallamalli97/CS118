
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
#include <sys/stat.h>
#include <math.h>

#define MAXLEN 524
#define MAXSEQ 25600

#define MINCWND 512
#define MAXCWND 10240
#define max(a,b) (a>b?a:b)

//fsefsd
struct udpheader {
	short sequence_number;
	int ack_number; 
	char ACK;
	char SYN;
	char FIN;
	char pad;
	short size;
} __attribute__((__packed__));

struct packet {
	struct udpheader packet_header;
	char payload[512];
	// char *payload;
};

int port;
char* host = NULL;
// char* test = "TESTING TESTING";
char* filename = NULL;

//need to exit gracefully after 10 seconds
int timeout = 0;

int abort_counter = 0;
// srand(time(0));
// short seqnum = rand() % (MAXSEQ + 1 - 0) + 0;
// int ack = 0;

int match(const char *string, const char *pattern)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB) != 0) return 0;
    int status = regexec(&re, string, 0, NULL, 0);
    regfree(&re);
    if (status != 0) return 0;
    return 1;
}

int send_SYN(int socket, struct packet p_syn, struct sockaddr_in servername){
	int n = sendto(socket, (struct packet*) &p_syn, sizeof(p_syn), 0, (struct sockaddr *) &servername, sizeof(servername));
	if(n <= 0){
		fprintf(stderr, "ERROR: unable to send initiation packet");
	}
	return n;
}


int shift_left(struct packet* p[]){
	int counter = 0;
	while(p[0] == NULL){
		if(counter > 9){
			break;
		}
		for(int i = 0; i < 9; i++){
			if(p[i] == NULL){
				struct packet* temp = p[i];
				p[i] = p[i+1];
				p[i+1] = temp;
			}
			else{
				break;
			}
		}
		counter++;
	}
	return 0;
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

	// Make array of packets to hold un acked packets that are sent. Remove when they are acked.
	struct packet* un_acked[10];

	// Initiate threeway handshake
	srand(time(0));
	short seqnum = rand() % (MAXSEQ + 1 - 0) + 0;
	int ack = 0;
	printf("sequence number for packet 1 being sent: %d\n", seqnum);

	struct udpheader packet_header = {seqnum, ack, 0, 1, 0, 0, 0}; // {seqnum, ack, ack_flag, SYN_flag, FIN_flag, padding, payload size}
	
	printf("size of seqnum: %lu, size of ack: %lu, size of ack flag: %lu, size of syn flag: %lu, size of fin flag: %lu. size of pad: %lu. size of psize: %lu\n", sizeof(packet_header.sequence_number), sizeof(packet_header.ack_number), sizeof(packet_header.ACK), sizeof(packet_header.SYN), sizeof(packet_header.FIN), sizeof(packet_header.pad), sizeof(packet_header.size));
	printf("%lu\n", sizeof(packet_header));


	// printf("%d\n", seqnum);
	struct packet p_syn = {packet_header};
	// n = sendto(sock, (struct packet*) &p_syn, sizeof(p_syn), 0, (struct sockaddr *) &servername, sizeof(servername));
	// if(n <= 0){
	// 	fprintf(stderr, "ERROR: unable to send initiation packet");
	// }
	int syn_acked = 0;
	struct timeval tv;
	int retval;
	fd_set rfds;

	struct packet * r_syn = malloc(sizeof(struct packet));

	while(!syn_acked){
		send_SYN(sock, p_syn, servername);
		un_acked[0] = &p_syn;

		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		retval = select(sock + 1, &rfds, NULL, NULL, &tv);

		if (retval == -1){
			fprintf(stderr, "ERROR: select() returned -1\n");
		}
		else if (retval){
			n = recvfrom(sock, r_syn, sizeof(*r_syn), 0, (struct sockaddr *) &servername, &len);
			if (n < 0){
				fprintf(stderr, "ERROR: recvfrom\n");
			}
			syn_acked = 1;
			un_acked[0] = NULL;
		}
		else{
			printf("retransmitting SYN\n");
			abort_counter++;
			if(abort_counter == 20){
				close(sock);
				printf("HOUSTON WE HAVE A PROBLEM, ABORT ABORT!\n");
				exit(1);
			}
		}
	}
	abort_counter = 0;

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

	// get size of file

	struct stat finfo;
    fstat(fileno(fp), &finfo);
    int file_size = finfo.st_size;
    printf("file size: %d\n", file_size);
    int exp_num_packets = (file_size + 512 - 1) / 512;
    printf("expected number of packets: %d\n", exp_num_packets);
    int packets_delivered = 0;

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

	// char rep_buf[512] = {0};

	// initialize cwnd to mincwnd (512)
	int cwnd = MINCWND;
	int ssthresh = 5120;
	int packets2send = cwnd / 512;
	int packets_awaiting_ack = 0;

	int packets_resend = 0;
	// int packets_resent = 0;
	
	int bytes_read;
	printf("entering sendto loop");
	int retransmit = 0;
	// int num_retransmitted_per_cycle = 0;
	struct packet * r = malloc(sizeof(struct packet));
			
	while(packets_delivered < exp_num_packets)
	{
		packets2send = cwnd / 512;

		if(packets_delivered + packets2send >= exp_num_packets){
				packets2send = exp_num_packets - packets_delivered;
		}

		while(packets2send != 0){

			// TO-DO: create array of packets that holds packets that aren't acked. Whenever a packet is sent, add it to the array 

			struct packet p;
			if(packets_resend > 0 && un_acked[packets_awaiting_ack] != NULL){
				// cwnd resets to 1, all packets to resend are backlogged
				// p = **(un_acked+packets_awaiting_ack);
				p = *un_acked[packets_awaiting_ack];
				seqnum = p.packet_header.sequence_number;
				// packets_resend--;
				// packets_resent++;
				// num_retransmitted_per_cycle++;
			}
			else{
				bytes_read = read(fileno(fp), &buf, 512);
				struct udpheader p_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, bytes_read};
				p.packet_header = p_header;
				memset(p.payload, 0, sizeof(p.payload));
				// p.payload = malloc(sizeof(char)+bytes_read);
				memcpy(p.payload, buf, bytes_read);
				int count_checker;
				for(count_checker = 0; count_checker < 10; count_checker++){
					if(un_acked[count_checker] == NULL){
						un_acked[count_checker] = &p;
						break;
					}
				}
			}
			printf("sequence number for packet being sent: %d\n", p.packet_header.sequence_number);

			int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &servername, sizeof(servername));
			if(packet_written <= 0){
				fprintf(stderr, "unable to write to socket");
				exit(1);
			}
			seqnum += bytes_read;
			if(seqnum > MAXSEQ){
				seqnum = 0;
			}
			packets2send--;
			packets_awaiting_ack++;
		}

		while(packets_awaiting_ack != 0){
			// struct packet * r = malloc(sizeof(struct packet));

			FD_ZERO(&rfds);
			FD_SET(sock, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 500000;
			retval = select(sock + 1, &rfds, NULL, NULL, &tv);

			if (retval == -1){
				fprintf(stderr, "ERROR: select() returned -1");
			}
			else if (retval){
				n = recvfrom(sock, r, sizeof(*r), 0, (struct sockaddr *) &servername, &len);
				if (n < 0){
					fprintf(stderr, "ERROR: recvfrom");
				}
				packets_delivered++;
				packets_awaiting_ack--;
				// if acked, removed from unacked array
				un_acked[packets2send] = NULL;
				// if its a retransmitted packet, decrement packets to resend
				if(packets_resend > 0){
					packets_resend--;
					// shift_left(&un_acked);
				}
				packets2send++;
				// if(num_retransmitted_per_cycle > 0){
				// 	num_retransmitted_per_cycle--;
				// }
				seqnum = r->packet_header.ack_number;
				ack = 0;
				ack_flag = 0;
				printf("sequence number for packet received: %d\n", seqnum);
				printf("ack number for packet received: %d\n", r->packet_header.ack_number);
				abort_counter = 0;
			}
			else{
				printf("timed out, need to retransmit\n");
				abort_counter++;
				// to resend all packets that are unacked after last acked
				int un_ackchecker;
				int count_unacks = 0;
				for(un_ackchecker = 0; un_ackchecker < 10; un_ackchecker++){
					if(un_acked[un_ackchecker] != NULL){
						count_unacks++;
					}
				}
				packets_resend = count_unacks;
				retransmit = 1;
				break;
			}
		}

		if(packets_delivered >= exp_num_packets){
			break;
		}

		// 10 second timeout, abort connection
		if(abort_counter == 20){
			close(sock);
			printf("HOUSTON WE HAVE A PROBLEM, ABORT ABORT!\n");
			exit(1);
		}

		// should be able to only do this if packets_resend > 0
		shift_left(&un_acked);
		if(!retransmit){
			if(cwnd < ssthresh){
				cwnd += 512;
			}
			else{
				cwnd += (512 * 512)/cwnd;
			}
			if(cwnd > MAXCWND){
				cwnd = MAXCWND;
			}
		}
		else{
			ssthresh = max(cwnd/2, 1024);
			cwnd = MINCWND;
		}
		retransmit = 0;
		packets_awaiting_ack = 0;
		printf("packets delivered: %d\n", packets_delivered);
		// num_retransmitted_per_cycle = 0;
	}
	// free the allocated memory
	// TO-DO make sure to free all mallocs
	free(r);


	// while((bytes_read = read(fileno(fp), &buf, 512)) != EOF){
	// 	packets2send = cwnd / 512;
	// 	if(bytes_read == 0){
	// 		break;
	// 	}
	// 	// printf("outer while loop bytes read: %d\n", bytes_read);
	// 	// if(bytes_read < 512){
	// 	// 	buf[bytes_read] = '\0';
	// 	// }
	// 	// printf("payload contains: %s\n", buf);
	// 	// printf("actual size of buffer (strlen): %lu\n", strlen(buf));
	// 	// while(strlen(buf) == 0){
	// 	// 	printf("buffer had zero bytes, trying to reread");
	// 	// 	bytes_read = read(fileno(fp), &buf, 512);
	// 		// printf("bytes read: %d\n", bytes_read);
	// 	// 	if(bytes_read < 512){
	// 	// 		buf[bytes_read] = '\0';
	// 	// 	}
	// 	// }
	// 	printf("packets 2 send at start of outer loop: %d\n", packets2send);
	// 	while(packets2send != 0){
	// 		// TO-DO: create array of packets that holds packets that aren't acked. Whenever a packet is sent, add it to the array 
	// 		if(bytes_read == 0){
	// 			printf("number of packets left to send: %d\n", packets2send);
	// 			break;
	// 		}
	// 		// printf("start of inner while loop bytes read: %d\n", bytes_read);
	// 		printf("bytes read: %d\n", bytes_read);
	// 		struct udpheader p_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, bytes_read};
	// 		struct packet p = {p_header};
	// 		memset(p.payload, 0, sizeof(p.payload));
	// 		// p.payload = malloc(sizeof(char)+bytes_read);
	// 		memcpy(p.payload, buf, bytes_read);
	// 		// printf("The size of payload is: %lu\n", sizeof(p.payload));
	// 		// memset(buf, 0, sizeof(buf));
	// 		printf("sequence number for packet being sent: %d\n", p.packet_header.sequence_number);
	// 		// printf(sizeof(p));
	// 		// void *out = buf;

	// 		printf("size of packet header sent: %lu\n", sizeof(p_header));
	// 		// while(bytes_read > 0){
	// 		int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &servername, sizeof(servername));
	// 		if(packet_written <= 0){
	// 			fprintf(stderr, "unable to write to socket");
	// 			exit(1);
	// 		}
	// 		seqnum += bytes_read;
	// 		if(seqnum > MAXSEQ){
	// 			seqnum = 0;
	// 		}
	// 		packets2send--;
	// 		packets_sent++;
	// 		if(packets2send > 0){
	// 			bytes_read = read(fileno(fp), &buf, 512);
	// 			// printf("inner while loop bytes read: %d\n", bytes_read);
	// 		}
	// 	}
	// 	// printf("number of acks to receive: %d\n", packets_sent);

	// 	// struct udpheader p_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0};
	// 	// struct packet p = {p_header};
	// 	// strncpy(p.payload, buf, sizeof(buf));
	// 	// // printf("%d\n", sizeof(buf));
	// 	// printf("sequence number for packet 2 being sent: %d\n", p.packet_header.sequence_number);
	// 	// printf(sizeof(p));
	// 	// void *out = buf;

	// 	// while(bytes_read > 0){


	// 	while(packets_sent != 0){
	// 		// implement select inside, break when timeout and retransmit (once packet is acked remove )
	// 		struct packet * r = malloc(sizeof(struct packet));
	// 		n = recvfrom(sock, r, sizeof(*r), 0, (struct sockaddr *) &servername, &len);
	// 		if (n < 0){
	// 			fprintf(stderr, "ERROR in recvfrom\n");
	// 			exit(1);
	// 		}
	// 		packets_sent--;
	// 		packets2send++;
	// 		seqnum = r->packet_header.ack_number;
	// 		ack = 0;
	// 		ack_flag = 0;
	// 		printf("sequence number for packet received: %d\n", seqnum);
	// 		printf("ack number for packet received: %d\n", r->packet_header.ack_number);
	// 		// printf("waiting for # of packets: %d\n", packets_sent);
	// 	}

	// 	//

	// 	if(packets_sent == 0){
	// 		if(cwnd < ssthresh){
	// 			cwnd += 512;
	// 		}
	// 		else{
	// 			cwnd += (512 * 512)/cwnd;
	// 		}
	// 		if(cwnd > MAXCWND){
	// 			cwnd = MAXCWND;
	// 		}
	// 	}
	// 	else{
	// 		cwnd = MINCWND;
	// 	}
	// 	// int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &servername, sizeof(servername));
	// 	// if(packet_written <= 0){
	// 	// 	fprintf(stderr, "unable to write to socket");
	// 	// 	exit(1);
	// 	// }
	// 	// clock_t before = clock();
	// 	// int milliseconds = 0;
	// 	// n = 0;
	// 	// timer(&sock, &timeout);
	// 	// int bytes_written = sizeof(buf);
	// 	// int n, len;
	// 	// struct packet * r = malloc(sizeof(struct packet));
	// 	// n = recvfrom(sock, r, sizeof(*r), 0, (struct sockaddr *) &servername, &len);
	// 	// clock_t difference = clock() - before;
	// 	// int milliseconds = difference * 1000 / CLOCKS_PER_SEC;
	// 	// printf("%d\n", milliseconds);
	// 	// seconds = milliseconds/1000;
	// 	// if(milliseconds % 500 == 0){
	// 	// 	printf("had to retransmit, 0.5 sec RTO\n");
	// 	// 	int packet_written = sendto(sock, (struct packet*) &p, sizeof(p), 0, (struct sockaddr *) &servername, sizeof(servername));
	// 	// 	if(packet_written <= 0){
	// 	// 		fprintf(stderr, "unable to write to socket\n");
	// 	// 		exit(1);
	// 	// 	}
	// 	// }
	// 	// if(milliseconds >= 10000){
	// 	// 	printf("10 second timeout, close socket and exit\n");
	// 	// 	close(sock);
	// 	// 	exit(1);
	// 	// }
	// 	// printf("escaped in %d milliseconds\n", milliseconds);
	// 	// n = recvfrom(sock, r, sizeof(*r), 0, (struct sockaddr *) &servername, &len);
	// 	// if (n < 0){
	// 	// 	fprintf(stderr, "ERROR in recvfrom\n");
	// 	// 	exit(1);
	// 	// }

	// 	// seqnum = r->packet_header.ack_number;
	// 	// ack = 0;
	// 	// ack_flag = 0;
	// 	// printf("sequence number for packet 2 received: %d\n", seqnum);
	// 	// printf("ack number for packet 2 received: %d\n", r->packet_header.ack_number);
	// 	// buf[n] = '\0';
	// 	// printf("Echo from server: %s\n", buf);

	// 	// bytes_read -= bytes_written;
	// 	// out += bytes_written;
	// 	// }
	// }

	// fclose(fown);
	// Send UDP Packet with FIN flag
	fin_flag = 1;
	struct udpheader fin_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, 0};
	struct packet f = {fin_header};
	n = sendto(sock, (struct packet*) &f, sizeof(f), 0, (struct sockaddr *) &servername, sizeof(servername));
	if (n < 0){
		fprintf(stderr, "ERROR: could not send fin packet");
	}
	printf("fin sent\n");


	struct packet * r_ack = malloc(sizeof(struct packet));
	n = recvfrom(sock, r_ack, sizeof(*r_ack), 0, (struct sockaddr *) &servername, &len);
	
	// expect to receive packet with ACK flag
	if(r_ack->packet_header.ACK != 1){
		fprintf(stderr, "ERROR: response from server to FIN did not have ACK.\n");
	}
	printf("ack received: %d\n", r_ack->packet_header.ack_number);
	// increment sequence number
	seqnum++;

	time_t startTime = time(NULL);
	printf("start time is: %ld\n", startTime);

	// while(1){
	// 	printf("difference is: %ld\n", time(NULL)-startTime);
	// }
	// while(time(NULL) - startTime < 2){
	// while(1){
	// 	printf("current time is: %ld\n", time(NULL));
	// 	printf("difference is: %ld\n", time(NULL)-startTime);
	struct packet * r_fin= malloc(sizeof(struct packet));
	n = recvfrom(sock, r_fin, sizeof(*r_fin), 0, (struct sockaddr *) &servername, &len);
	if(n > 0){
		printf("received fin, value of n is: %d\n", n);
	}
	if(n > 0){
		printf("checking fin packet from server\n");
		if(r_fin->packet_header.FIN == 1){
			printf("fin packet has fin flag up\n");
			// fprintf(stderr, "ERROR: response from server to FIN did not have ACK.\n");
			ack_flag = 1;
			fin_flag = 0;
			ack = r_fin->packet_header.sequence_number;
			struct udpheader fin_ack_header = {seqnum, ack, ack_flag, syn_flag, fin_flag, 0, 0};
			struct packet ack = {fin_ack_header};
			int fin_ack_pack = sendto(sock, (struct packet*) &ack, sizeof(ack), 0, (struct sockaddr *) &servername, sizeof(servername));
			if (fin_ack_pack < 0){
				fprintf(stderr, "ERROR: could not send ack packet for server fin");
			}
			printf("ack was sent\n");
		}
	}
	// }

	// n = recvfrom(sock, (char *) buf, MAXLEN, 0, &servername, &len);
	// if (n < 0)
	// 	fprintf(stderr, "ERROR in recvfrom");
	// buf[n] = '\0';
	close(sock);
	return 0;

}