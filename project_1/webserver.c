//Name: Chaitanya Pedada
//UID: 404729909
//Email: cpedada@ucla.edu

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/select.h>
#include <mcrypt.h>

void sighandler(int);

int port;
char *program = "/bin/bash";
pid_t pid;
int keyfd;
MCRYPT td, dtd;
char key[16];
char *IV;
int keysize=16; /* 128 bits */

void exitData(pid_t p, int sock){
	int status;
	if(waitpid(p, &status, 0) == -1){
		fprintf(stderr, "Waitpid error. %s\n", strerror(errno));
		exit(1);
	}
	fprintf(stderr, "SHELL EXIT SIGNAL=%d STATUS=%d\n", WTERMSIG(status), WEXITSTATUS(status));
	close(sock);
	exit(1);
}

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

void shutdown_mcrypt(){
	mcrypt_generic_deinit(td);
	mcrypt_module_close(td);
	mcrypt_generic_deinit(dtd);
	mcrypt_module_close(dtd);
}

//https://www.gnu.org/software/libc/manual/pdf/libc.pdf, pg. 488-489

int main(int argc, char* argv[]){
	
	static struct option longopts[] = {
    	{"port", required_argument, 0, 'p'},
    	{"encrypt", required_argument, 0, 'e'},
    	{0, 0, 0, 0}
	};

	int val;
	int option_index = 0;
	char c[512];
	key[0] = 0;

	while((val = getopt_long(argc, argv, "", longopts, &option_index)) != -1){
		switch(val){
			case 'p':
				port = atoi(optarg);
				break;
			case 'e':
				keyfd = open(optarg, O_RDONLY);
				keysize = read(keyfd, &key[0], keysize);
				write(STDOUT_FILENO, &key[0], keysize);
				//https://linux.die.net/man/3/mcrypt
				td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
				dtd = mcrypt_module_open("twofish", NULL, "cfb", NULL);
				if (td==MCRYPT_FAILED) {
     				return 1;
  				}
  				IV = malloc(mcrypt_enc_get_iv_size(td));
  				int i;
  				for (i=0; i< mcrypt_enc_get_iv_size( td); i++) {
  					IV[i]= i;
  				}
  				mcrypt_generic_init(td, &key[0], keysize, IV);
  				mcrypt_generic_init(dtd, &key[0], keysize, IV);
  				atexit(shutdown_mcrypt);
  				break;
			default:
				fprintf(stderr, "Unrecognized argument. %s\r\n", strerror(errno));
				exit(1);
		}
	}
	
	int sock;
	struct sockaddr_in clientname;
	socklen_t size;

	sock = make_socket();
	if(listen (sock, 1) < 0){
		fprintf(stderr, "Error while socket on server tried to listen. %s\r\n", strerror(errno));
		exit(1);
	}

	char lf = '\n';
	int c2p[2];
	int p2c[2];
	int newsock;
	size = sizeof(clientname);
	newsock = accept(sock, (struct sockaddr *) &clientname, &size);
	if(newsock < 0){
		fprintf(stderr, "Socket acception failed. %s\r\n", strerror(errno));
		exit(1);
	}

	if (pipe (c2p))
	{
		fprintf (stderr, "Error: Pipe failed. %s\r\n", strerror(errno));
		exit(1);
	}
	if (pipe (p2c))
	{
		fprintf (stderr, "Error: Pipe failed. %s\r\n", strerror(errno));
		exit(1);
	}
	pid = fork();

	if (pid == (pid_t) 0){ //pid_t data type represents process IDs
		//this is the child process

		/*
			p2c
			P ---------> C
			[0]			[1]
			p2c[0] must become stdin
			c2p
			P <--------- C
			[1]			[0]
			c2p[1] must become stdout
		*/

		close(c2p[0]); //close read end of child 2 parent pipe
		close(p2c[1]); //close write end of parent 2 child pipe

		dup2(p2c[0], STDIN_FILENO); //redirect to STDIN
		dup2(c2p[1], STDOUT_FILENO); //redirect to STDOUT
		dup2(c2p[1], STDERR_FILENO); //redirect to STDERR
		
		close(c2p[1]); //no longer need the pipe ends since dup(s)
		close(p2c[0]);

		//execute shell
		if(execlp(program, program, (char *) NULL) == -1){ //lp used because number of arguments (which is zero) is known and and array of parameters is unnecessary
			fprintf(stderr, "Error: %s\r\n", strerror(errno));
			exit(1);
		}

	}
	else if (pid < -1){
		//fork failed
		fprintf(stderr, "Error: %s\r\n", strerror(errno));
		exit(1);
	}
	else{
		//this is the parent process

		/*
			p2c
			P ---------> C
			[0]			[1]
			need to write to p2c[1]
			c2p
			P <--------- C
			[1]			[0]
			need to poll c2p[0]
		*/

		close(c2p[1]);
		close(p2c[0]);

		//used http://www.unixguide.net/unix/programming/2.1.2.shtml for reference
		signal(SIGINT, sighandler);
		signal(SIGPIPE, sighandler);
		struct pollfd poll_list[2];
		int retval;
		poll_list[0].fd = newsock;
		poll_list[1].fd = c2p[0];
		poll_list[0].events = POLLIN;
		poll_list[1].events = POLLIN;

		while(1){
			retval = poll(poll_list, (unsigned long) 2, 0);
			if(retval < 0){
				fprintf(stderr, "Error: %s\n", strerror(errno));
				exit(1);
			}

			if((poll_list[0].revents&POLLIN) == POLLIN){
				//printf("%s\r\n", "Input from keyboard");
				int bytesRead = read(newsock, &c, 512);
				if(bytesRead < 0){
					fprintf(stderr, "Unable to read from client. %s\r\n", strerror(errno));
					close(p2c[1]);
					exitData(pid,sock);
				}
				else if (bytesRead == 0){
					/* End of File */
					fprintf(stderr, "Reached end of file.\n");
					close(p2c[1]);
					exitData(pid, sock);
				}
				int i;
				for(i = 0; i < bytesRead; i++){
					//write(STDOUT_FILENO, &c[i], 1);
					if(key[0]!=0){
                		mdecrypt_generic(dtd, &c[i], 1);
                	}
                	write(STDOUT_FILENO, &c[i], 1);
					switch(c[i]){
						case 0x04:
							//printf("%s\r\n", "Hit the kill switch");
							close(p2c[1]);
							exitData(pid, sock);
							return 0;
						case 0x03:
							kill(pid, SIGINT);
							break;
						case 0x0D:
						case 0x0A:
							if(write(p2c[1], &lf, 1) == -1){
								fprintf(stderr, "Error: %s\r\n", strerror(errno));
								exit(1);
							}
							break;
						default:
							if(write(p2c[1], &c[i], 1) == -1){
								fprintf(stderr, "Error: %s\r\n", strerror(errno));
								exit(1);
							}	
					}

				}
				memset(c, 0, bytesRead);
			}
			else if(((poll_list[0].revents&POLLHUP) == POLLHUP) || ((poll_list[0].revents&POLLERR) == POLLERR)){
				exit(0);
			}

			if((poll_list[1].revents&POLLIN) == POLLIN){
				//printf("%s\r\n", "Input from shell");
				int bytesRead = read(c2p[0], &c, 512);
				if(bytesRead < 0){
					fprintf(stderr, "Unable to read from shell. %s\r\n", strerror(errno));
					exit(1);
				}
				else if (bytesRead == 0){
					/* End of File */
					exitData(pid, sock);
				}
				int i;
				for(i = 0; i < bytesRead; i++){
					if(key[0]!=0){
                        	mcrypt_generic(td, &c[i], 1);
                    }
                    write(STDOUT_FILENO, &c[i], 1);
					if(write(newsock, &c[i], 1) == -1){
						fprintf(stderr, "Error: %s\n", strerror(errno));
						exit(1);
					}
				}
				
			}
			else if(((poll_list[1].revents&POLLHUP) == POLLHUP) || ((poll_list[1].revents&POLLERR) == POLLERR)){
				exitData(pid, sock);
			}
		}
	}
	exit(0);
}

void sighandler(int signum) {
  if(signum == SIGINT){
    fprintf(stderr, "%s", "SIGINT caught.\n");
    fprintf(stderr, "%s", "Kill command for pid sent. \n");
  }
  if(signum == SIGPIPE){
  	fprintf(stderr, "%s", "SIGPIPE caught.\n");
  }
  exit(0);
}
