#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define PORT 8080
#define BUFFER_SIZE 100000
#define MAX_LISTEN_QUEUE 16
#define SEND_SIZE 70000

/*The header in http packet */
char imageheader[] = 
	"HTTP/1.1 200 Ok\r\n"
	"Content-Type: image/jpeg\r\n\r\n";
char htmlheader[] = 
	"HTTP/1.1 200 Ok\r\n"
	"Content-Type: text/html; charset=UTF-8\r\n\r\n";


/*handle child process terminated*/
void sigchld_handler(int signal){
	pid_t pid;
	int   stat;
	
	while((pid = waitpid(-1 ,&stat ,WNOHANG )) > 0)	//wait child process to prevent zombies 
		printf("child %d terminated\n\n", pid);	
	printf("==================================================\n\n");			
	return;
}	

/*get file from http packet and save it to local*/
int writeFile(char *ptr,int fd_client){
	int index = 0;
	char name[256];
	char boundary[256];
	char *end = NULL;
	int size = 0;
	
	printf("Writing file...\n");
	//getboundary
	ptr = strstr(ptr,"boundary=");
	if(!ptr) return -1;
	ptr += 9;			// boundary=
	boundary[0] = '-';
	boundary[1] = '-';
	index = 2;
	while(*ptr != '\r'){
		boundary[index++] = *ptr++;
	}
	boundary[index] = '\0';	
	
		
	//getSize
	ptr = strstr(ptr,"Content-Length: ");
	if(!ptr) return -1;
	size = atoi(ptr + 16);

	//getName
	ptr = strstr(ptr,"filename=");
	if(!ptr) return -1;
	ptr += 10;			// filename="
	
	index = 0;
	while(*ptr != '"'){
		name[index++] = *ptr++;
	}
	name[index] = '\0';	

	
	//getContent	
	FILE *fp = fopen(name, "wb+");
	ptr = strstr(ptr,"Content-Type:");
	if(!ptr) return -1;
	ptr = strstr(ptr,"\r\n\r\n");
	ptr += 4;
	end = strstr(ptr,boundary);
	//end != null,text format
	//end == null,not text format

	if(end){
		end -= 2; 		// \r\n
		while(ptr != end){
			fwrite(ptr++,1,1,fp);
		}	
	} 
	else{
		fwrite(ptr,1,size,fp);
	}

	fclose(fp);
	printf("File name : %s\n",name);
	printf("File size : %d\n",size);	
	return 0;
	
}

int main(int argc, char *argv[]){

	struct sockaddr_in server_addr, client_addr;
	socklen_t sin_len = sizeof(client_addr);	//length of cnt socket
	int fd_server , fd_client;			//file descriptor
	char buf[BUFFER_SIZE];
	int fd;
	int on = 1;
	
	//Process : build socket => bind port => listen => accept
	
	fd_server = socket(AF_INET, SOCK_STREAM,0);
	if (fd_server < 0){
		perror("socket");
		exit(1);
	}
	
	setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;	
	server_addr.sin_port = htons(PORT);		//port number
	
	if (bind(fd_server, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){
		perror("Bind failed");
		close(fd_server);
		exit(1);
	}
	
	printf("Server start\n");
	if (listen(fd_server,MAX_LISTEN_QUEUE) == -1){
		perror("Listen failed");
		close(fd_server);
		exit(1);
	}
	
	signal(SIGCHLD ,sigchld_handler);	//prevent zombies 
	
	while(1){
		fd_client = accept(fd_server, (struct sockaddr *) &client_addr, &sin_len);
		
		if (fd_client == -1){
			perror("Connection failed\n");
			continue;
		}
		
		printf("Client%d connection\n",fd_client);
		
		//child process
		if (!fork()){ 
			close(fd_server);
			memset(buf,0,BUFFER_SIZE);
			int byte = read(fd_client, buf, BUFFER_SIZE - 1);
			//printf("CONTENT OF HTTP PACKET\n%s\n", buf);
			
			//handle http packet from client
			//1.get cover image
			//2.handle file from client
			//3.other
			if (!strncmp(buf,"GET /duoduo.png",13)){
				printf("Request : GET\n");
				write(fd_client, imageheader, sizeof(imageheader) - 1);
				fd = open("duoduo.png",O_RDONLY);
				sendfile(fd_client,fd,NULL,SEND_SIZE);
				close(fd);
			}
			else if (!strncmp(buf,"POST",4)){
				printf("Request : POST\n");
				if(writeFile(buf,fd_client) == -1){
					perror("Write file failed\n");
				}
				
				printf("Reload web\n");
				write(fd_client, htmlheader, sizeof(htmlheader) - 1);
				fd = open("index.html",O_RDONLY);
				sendfile(fd_client,fd,NULL,SEND_SIZE);
				close(fd);
			}
			else{
				printf("Reload web\n");
				write(fd_client, htmlheader, sizeof(htmlheader) - 1);
				fd = open("index.html",O_RDONLY);
				sendfile(fd_client,fd,NULL,SEND_SIZE);
				close(fd);
				
			}
			close(fd_client);
	
			exit(0);
		}
		//parent process
		close(fd_client);
	}

	return 0;
}
