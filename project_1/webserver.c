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

#define MAX_LENGTH 8332
#define FILE_PATH_SIZE 10000

int port;
FILE* fp = NULL;
char *addr;

int make_socket(){
	int sock;
	struct sockaddr_in name;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		fprintf(stderr, "Error making socket. %s\r\n", strerror(errno));
		exit(1);
	}
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){
    	fprintf(stderr, "setsockopt(SO_REUSEADDR) failed. %s\n", strerror(errno));
    	exit(1);
	}
	name.sin_family = AF_INET;
	name.sin_port = htons(port);
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(sock, (struct sockaddr *) &name, sizeof(name))){
		fprintf(stderr, "Unable to bind socket. %s\r\n", strerror(errno));
		exit(1);
	}
	return sock;
}

//Referenced make_socket from https://www.gnu.org/software/libc/manual/html_node/Inet-Example.html


//https://www.gnu.org/software/libc/manual/pdf/libc.pdf, pg. 488-489

int main(int argc, char* argv[]){
	
	if (argc < 2){
		fprintf(stderr, "invalid arguments. Must be port and file. %s\r\n", strerror(errno));
	}

	port = atoi(argv[1]);
	int sock;
	struct sockaddr_in clientname;
	socklen_t size;

	sock = make_socket();
	if(listen (sock, 1) < 0){
		fprintf(stderr, "Error while socket on server tried to listen. %s\r\n", strerror(errno));
		exit(1);
	}

	int newsock;
	size = sizeof(clientname);
	char input[MAX_LENGTH] = {0};
	char output[MAX_LENGTH] = {0};

	while(1){
		newsock = accept(sock, (struct sockaddr *) &clientname, &size);
		if(newsock < 0){
			fprintf(stderr, "Socket acception failed. %s\r\n", strerror(errno));
			exit(1);
		}
		int bytesRead = read(newsock, &input, MAX_LENGTH);
		if(bytesRead < 0){
			fprintf(stderr, "Unable to read from client. %s\r\n", strerror(errno));
			exit(1);
		}

		fprintf(stdout, "%s\n", input);

		//get the file path, start at the 4th char because first 3 (0-2) is GET, 3 is a space

		char filepath[FILE_PATH_SIZE];
		strcpy(filepath, ".");
		// fprintf(stdout, "%s\n", filepath);

		int versionStart = 0;
		for(int i = 1; i < FILE_PATH_SIZE; i++){
			if(input[i+3] == ' '){
				versionStart = i+4;
				break;
			}
			else{
				filepath[i] = input[i+3];
			}
		}

		// printf("%s\n", filepath);

		char version[10];
		for(int i = 0; i < 9; i++){
			version[i] = input[versionStart+i];
		}
		// version[9] = '\n';
		char response[MAX_LENGTH] = "";
		
		// char stat[9] = " 200 OK\n";

		strcpy(response, version);

		response[strlen(response) -1 ] = '\0';
		// char firstline[100];
		// printf("%s 200 OK \n", response);
		write(newsock, response, strlen(response));
		write(newsock, " 200 OK\n", 8);

		// // response = version;
		// char stat[MAX_LENGTH] = " 200 OK\n";
		// strcat(response, stat);
		// printf("%s\n 200 OK\n", version);

		time_t t = time(NULL);
		struct tm tm = *localtime(&t);

		char time[80];
		strftime(time, sizeof(time), "%a, %d %b %Y %X %Z\n", &tm);
		// printf("Date: %s\n", time);

		//empty response array
		memset(response, 0, strlen(response));

		strcat(response, "Date: ");
		strcat(response, time);

		write(newsock, response, strlen(response));
		// printf("%s", response);
		// fprintf(stdout, "%s\n", version);
		// fprintf(stdout, "%s\n", filepath);

		// open file
		//TO-DO show 404 not found error in HTTP response
		fp = fopen(filepath, "r");
		if(fp == NULL){
			printf("cannot open\n");
			exit(1);
		}

		// https://techoverflow.net/2013/08/21/how-to-get-filesize-using-stat-in-cc/
		struct stat st;
		if(stat(filepath, &st) != 0){
			fprintf(stderr, "could not obtain file size");
			exit(1);
		}
		char content_length[100];
		sprintf(content_length, "Content-Length: %lld\n", st.st_size);
		// fprintf(stdout, "%lld\n", st.st_size);
		content_length[strlen(content_length)] = '\0';


		write(newsock, content_length, strlen(content_length));
		// printf("%s\n", content_length);
		// printf("%s\n", filepath);

		// memset(content_length, 0, strlen(content_length));
		char filetype[6] = {0};

		// memset(filetype, 0, strlen(filetype));
		// printf("%s\n", filetype);
		char content_type[100];
		int typeIndex = 0;
		int type = 0;
		int filepathLength = strlen(filepath);

		for(int i = 1; i < filepathLength; i++){
			if(filepath[i] == '.'){
				type = 1;
			}
			if(type == 1){
				filetype[typeIndex] = filepath[i];
				typeIndex++;
			}
		}
		filetype[strlen(filetype)] = '\0';
		sprintf(content_type, "Content-Type: %s\n", filetype);

		write(newsock, content_type, strlen(content_type));

		char newline[1];
		newline[0] = '\n';

		write(newsock, newline, 1);

		int bytes_read;
		while((bytes_read = read(fileno(fp), &output, MAX_LENGTH)) != EOF){
			void *p = output;
			while(bytes_read > 0){
				int bytes_written = write(newsock, p, bytes_read);
				if(bytes_written <= 0){
					printf("unable to write to socket");
					exit(1);
				}
				bytes_read -= bytes_written;
				p += bytes_written;
			}
		}
	}

}
