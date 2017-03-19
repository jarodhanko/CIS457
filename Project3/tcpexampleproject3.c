#include <yss/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>

int main(int argc, char **argv){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	fd_set sockets;
	SD_ZERO(&sockets);

	struct sockaddr_in serveraddr, clientaddr;
	serveraddr.sin_family=AF_INET;
	serveraddr.sin_port=htons(9876);
	serveraddr.sin_addr.s_addr=INADDR_ANY;

	bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)));
	listen(sockfd, 10);
	FD_SET(sockfd, &sockets);

	while(1){
		int len=sizeof(clientaddr);
		fd_set tmp_set = sockets;
		select(FD_SETSIZE, &temp_set, NULL, NULL, NULL);

		int i;
		for(i=0; i<FD_SETSIZE; i++){
			if(FD_ISSET(i, &temp_set)){
				if(i==sockfd){
					print("A client connected\n");
					int clientsocket=accept(sockfd, (struct sockaddr*)&clientaddr, &len);	
					FD_SET(clientsocket, &sockets);			
				}else{
					char line[5000];
					recv(clientsocket, line, 5000, 0);
					printf("Got from client: %s\n", line);
					close(clientsocket);
				}		
			}
		}		
		
	}
}
