#include <arpa/inet.h>
#include <errno.h>

#include <ifaddrs.h>
#include <inttypes.h>

#include <netinet/ether.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netpacket/packet.h> 
#include <net/ethernet.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h> 
#include <sys/types.h>

#include <unistd.h>





/* STRUCTS **/
struct aarp {
	struct ether_header eth_header;
	struct ether_arp arp_header;
} __attribute__ ((__packed__));

struct iicmp{
	struct ether_header eth_header;
	struct iphdr ip_header;
	struct icmphdr icmp_header;
} __attribute__ ((__packed__));

struct routing_table {
	u_int32_t network;
	int prefix;
	u_int32_t hop;
	char interface[8];
	struct routing_table *next;
};

struct interface {
	char *name;
	u_int8_t mac_addrs[6];
	u_int8_t ip_addrs[4];
	int packet_socket;
	struct interface *next;
};
struct interface *interfaceList;




/* PROTOTYPES **/
void load_table(struct routing_table **rtable, char *filename);
void print_ETHERTYPE_ARP(struct aarp *request);
void print_ETHERTYPE_IP(struct iicmp reply);
void clearArray(char *array);





/****************************************************************************************
* Checksum
****************************************************************************************/
//icmp checksum calculator from
//source: http://www.microhowto.info/howto/calculate_an_internet_protocol_checksum_in_c.html
u_int16_t ip_checksum(void* vdata,size_t length) {
    // Cast the data pointer to one that can be indexed.
    char* data=(char*)vdata;

    // Initialise the accumulator.
    u_int32_t acc=0xffff;

    // Handle complete 16-bit blocks.
	size_t i;
    for (i=0;i+1<length;i+=2) {
        u_int16_t word;
        memcpy(&word,data+i,2);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Handle any partial block at the end of the data.
    if (length&1) {
        u_int16_t word=0;
        memcpy(&word,data+length-1,1);
        acc+=ntohs(word);
        if (acc>0xffff) {
            acc-=0xffff;
        }
    }

    // Return the checksum in network byte order.
    return htons(~acc);
}





/****************************************************************************************
* MAIN
****************************************************************************************/
int main(int argc, char **argv){
 	
	//get list of interfaces (actually addresses)
  	struct ifaddrs *ifaddr, *tmp;
  	if(getifaddrs(&ifaddr)==-1){
    	perror("getifaddrs");
    	return 1;
  	}
  	//have the list, loop over the list
  	for(tmp = ifaddr; tmp!=NULL; tmp=tmp->ifa_next){
    	//Check if this is a packet address, there will be one per
    	//interface.  There are IPv4 and IPv6 as well, but we don't care
    	//about those for the purpose of enumerating interfaces. We can
    	//use the AF_INET addresses in this list for example to get a list
    	//of our own IP addresses
		
		// All the eth-# interfaces.
		if (strcmp(tmp->ifa_name, "lo") != 0){	
			// Look to see if we already have the interface.
			struct interface *tempInterface, *prevInterface;
			int haveInterface = 0;
			for(tempInterface = interfaceList; tempInterface != NULL; tempInterface = tempInterface->next){
			
				if (strcmp(tempInterface->name, tmp->ifa_name)==0){
					haveInterface = 1;
					break;
				}
				prevInterface = tempInterface;
			}
			if (haveInterface == 0){
				tempInterface = malloc(sizeof(struct interface));
				tempInterface->name = tmp->ifa_name;
				tempInterface->next = NULL;
				if (interfaceList == NULL){
					interfaceList = tempInterface;
				}
				else {
					prevInterface->next = tempInterface;
				}
			}
			if (tmp->ifa_addr->sa_family == AF_INET){
				// Store the ip address into the interface.
				memcpy(tempInterface->ip_addrs, &((struct sockaddr_in*) tmp->ifa_addr)->sin_addr.s_addr, 
										  sizeof(((struct sockaddr_in*) tmp->ifa_addr)->sin_addr.s_addr));		
			}
			else if (tmp->ifa_addr->sa_family == AF_PACKET){
				//create a packet socket
				//AF_PACKET makes it a packet socket
				//SOCK_RAW makes it so we get the entire packet
				//could also use SOCK_DGRAM to cut off link layer header
				//ETH_P_ALL indicates we want all (upper layer) protocols
				//we could specify just a specific one
				int packet_socket;
				packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
				printf("packet_socket: %d \n", packet_socket);
				if(packet_socket<0){
	 	 			perror("socket");
	  				return 2;
				}
				//Bind the socket to the address, so we only get packets
				//recieved on this specific interface. For packet sockets, the
				//address structure is a struct sockaddr_ll (see the man page
				//for "packet"), but of course bind takes a struct sockaddr.
				//Here, we can use the sockaddr we got from getifaddrs (which
				//we could convert to sockaddr_ll if we needed to)
				if(bind(packet_socket,tmp->ifa_addr,sizeof(struct sockaddr_ll))==-1){
	  				perror("bind");
				}

				struct timeval tv;
				tv.tv_sec = 0;
				tv.tv_usec = 1000;
				if(setsockopt(packet_socket,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv))<0){
					perror("Error");
				}

				// Put the packet socket into the interface.
				tempInterface->packet_socket = packet_socket;

				// Store the mac address into the interface.
				int index;
	  			for (index = 0; index < 6; index++){
		 		 	tempInterface->mac_addrs[index] = ((struct sockaddr_ll*) tmp->ifa_addr)->sll_addr[index];
	  			}
			}
      	}
  	}
  	//free the interface list when we don't need it anymore
  	freeifaddrs(ifaddr);

  	//loop and recieve packets. We are only looking at one interface,
  	//for the project you will probably want to look at more (to do so,
  	//a good way is to have one socket per interface and use select to
  	//see which ones have data)
  	struct routing_table *rtable = malloc(sizeof(struct routing_table));
  	load_table(&rtable, argv[1]);

	struct interface *tempInterface = interfaceList;
	printf("-----------\n");
	while(tempInterface != NULL){
	
		printf("Interface: %s \n", tempInterface->name);
		printf("MAC  addr: %s \n", ether_ntoa((struct ether_addr*)tempInterface->mac_addrs));
		printf("IP   addr: %02X.%02X.%02X.%02X \n",tempInterface->ip_addrs[0], tempInterface->ip_addrs[1],
										  			 tempInterface->ip_addrs[2], tempInterface->ip_addrs[3]);
		printf("pack_sock: %d \n", tempInterface->packet_socket);
		printf("-----------\n");
		tempInterface = tempInterface->next;
	}
	struct routing_table *tempRtable = rtable;
	if(tempRtable == NULL)
		printf("NO TABLE");
	while(tempRtable != NULL){
	
		printf("Network  : %X \n", tempRtable->network);
		printf("Prefix   : %d \n", tempRtable->prefix);
		printf("Hop      : %X \n", tempRtable->hop);
		printf("Interface: %s \n", tempRtable->interface); 
		printf("-----------\n");
		tempRtable = tempRtable->next;
	}



  	printf("Ready to recieve now\n");
  	while(1){
		
		// Loop through all interfaces for packets.
		struct interface *tempInterface;		
		for(tempInterface = interfaceList; tempInterface != NULL; tempInterface = tempInterface->next){		

  			char buf[1500];
    		struct sockaddr_ll recvaddr;
    		socklen_t recvaddrlen=sizeof(struct sockaddr_ll);
    		//we can use recv, since the addresses are in the packet, but we
    		//use recvfrom because it gives us an easy way to determine if
    		//this packet is incoming or outgoing (when using ETH_P_ALL, we
    		//see packets in both directions. Only outgoing can be seen when
    		//using a packet socket with some specific protocol)
   		
			int n = recvfrom(tempInterface->packet_socket, buf, 1500,0,(struct sockaddr*)&recvaddr, &recvaddrlen);
 		
			//ignore outgoing packets (we can't disable some from being sent
    		//by the OS automatically, for example ICMP port unreachable
    		//messages, so we will just ignore them here)
    		if(recvaddr.sll_pkttype==PACKET_OUTGOING)
      			continue;

			// Timed out.
			if(n == -1){
				continue;
			}
    		//start processing all others
    		printf("Got a %d byte packet\n", n);
		
			
   	 
			
	
			if((ntohs(recvaddr.sll_protocol) == ETH_P_ARP) && n > -1){
				struct aarp *request;
		
				request = ((struct aarp*)&buf);
				
				// Print the request contents.	
				print_ETHERTYPE_ARP(request);

				// Create reply structure.
				struct aarp reply = *request;
	
				// IS THIS FOR US?
				//int our_IP = 0;
				//if (request->eth_header.ether_dhost == our_IP){
				
				//}



				// Copy info to reply.
				memcpy(reply.eth_header.ether_shost, tempInterface->mac_addrs, ETH_ALEN);		
				memcpy(reply.eth_header.ether_dhost, request->eth_header.ether_shost, ETH_ALEN);
				reply.arp_header.ea_hdr.ar_op=htons(ARPOP_REPLY);
				memcpy(reply.arp_header.arp_sha, tempInterface->mac_addrs, 6);
				memcpy(reply.arp_header.arp_spa, tempInterface->ip_addrs, 4);
				memcpy(reply.arp_header.arp_tha, request->arp_header.arp_sha, ETH_ALEN);
				memcpy(reply.arp_header.arp_tpa, request->arp_header.arp_spa, 4);
		
				// Send the reply packet.	
				send(tempInterface->packet_socket, &reply, sizeof(reply), 0);
				printf("Sent ARP reply");

			}else if((ntohs(recvaddr.sll_protocol) == ETH_P_IP) && n > -1){
				
		
				char buf2[1500];
				memcpy(buf2, buf, sizeof(buf));
		
				struct iicmp request2;
				unsigned char *data;
				request2 = *((struct iicmp*)&buf2);
				int datalength = ntohs(request2.ip_header.tot_len) - sizeof(request2.ip_header) - 
																	 sizeof(request2.icmp_header);

				if(datalength > 0){
					data = malloc(datalength);
					memcpy(data, buf2 + sizeof(request2), datalength);
				}
				printf("\n THE DATA LENGTH IS %lu\n", sizeof(&data));
				unsigned char tmp3[] = {buf2[26], buf2[27], buf2[28], buf2[29]};
				unsigned char tmp4[] = {buf2[30], buf2[31], buf2[32], buf2[33]};
		
				// Create reply structure.
				struct iicmp reply;

				// Copy info to reply.
				memcpy(&reply, &request2, sizeof(request2));
				memcpy(&reply.eth_header.ether_shost, tempInterface->mac_addrs, ETH_ALEN);
				memcpy(&reply.eth_header.ether_dhost, request2.eth_header.ether_shost, ETH_ALEN);
				memcpy(&reply.ip_header.daddr, tmp3, 4);
				memcpy(&reply.ip_header.saddr, tmp4, 4);
				reply.icmp_header.type = ICMP_ECHOREPLY;
				reply.icmp_header.checksum = 0;
				unsigned char ptr[sizeof(reply.icmp_header) + datalength];
				memcpy(ptr, &reply.icmp_header, sizeof(reply.icmp_header));
				memcpy(ptr + sizeof(reply.icmp_header), data, datalength);
				reply.icmp_header.checksum = ip_checksum(&ptr, sizeof(ptr));
		
				// Print the reply contents.
				print_ETHERTYPE_IP(reply);		
	
				unsigned char result[sizeof(reply) + datalength];
				memcpy(result, &reply, sizeof(reply));
				memcpy(result + sizeof(reply), data, datalength);
		
				send(tempInterface->packet_socket, &result, sizeof(result), 0);
			}
		}
  	}
  	// Exit main
  	return 0;
}





/****************************************************************************************
* LOAD ROUTING TABLE  ---- FIX ME ---- 
****************************************************************************************/
void load_table(struct routing_table **rtable, char *filename){
	
	FILE *fp = fopen(filename, "r");  	
  	if (fp == NULL){
    	printf("Could Not Open File");
    	exit(1);
	}
	
	(*rtable)->next = NULL;
	struct routing_table *tempRtable, *tmpRtable = NULL;
	tempRtable = (*rtable);
	
	int caseNum = 0;
	int i;
	char item[9];
	int index = 0;
	char c;
	while ((c = fgetc(fp)) != EOF){

		


		item[index++] = c;
		if (c == '/'){				// Network
			printf("NEW NETWORK\n");
			item[--index] = '\0';
			tempRtable->network = (u_int32_t)atoi(item);			
			index = 0;
			for(i = 0; i < 9; i++){
				item[index] = '\0';
			}
			caseNum++;
		}
		else if (c == ' '){
			if(caseNum == 1){  		// Prefix
				printf("NEW PREFIX\n");
				item[--index] = '\0';
				tempRtable->network = atoi(item);
				index = 0;
				caseNum++;
			}
			else if (caseNum == 2){ // Hop
				printf("NEW HOP\n");
				item[--index] = '\0';
				if(item[--index] == '-')
					tempRtable->network = -1;
				else 
					tempRtable->network = (u_int32_t)atoi(item);
				index = 0;
				caseNum++;
			}
		}
		else if (c == '\n' || c == EOF){		 // Interface
			printf("NEW LINE\n");
			item[--index] = '\0';
			memcpy(&tempRtable->network, item, 8);
			index = 0;
			if ((*rtable) == NULL)
				(*rtable) = tempRtable;
			else {
			}
			tempRtable->next = NULL;
			for(tmpRtable = (*rtable); tmpRtable != NULL; tmpRtable = tmpRtable->next){			
			
			}
			tmpRtable = tempRtable;
			caseNum = 0;
		}
	}
	

	fclose(fp); 
}






/****************************************************************************************
* print_ETHERTYPE_ARP
****************************************************************************************/
void print_ETHERTYPE_ARP(struct aarp *request){
	printf("ETHER DEST: %02X%02X%02X%02X%02X%02X \n", request->eth_header.ether_dhost[0],
				  request->eth_header.ether_dhost[1], request->eth_header.ether_dhost[2],
				  request->eth_header.ether_dhost[3], request->eth_header.ether_dhost[4],
				  request->eth_header.ether_dhost[5]);
	printf("ETHER SRC: %02X%02X%02X%02X%02X%02X \n", request->eth_header.ether_shost[0],
				 request->eth_header.ether_shost[1], request->eth_header.ether_shost[2],
				 request->eth_header.ether_shost[3], request->eth_header.ether_shost[4],
				 request->eth_header.ether_shost[5]);
	printf("ETHER TYPE: %02X \n\n", ntohs(request->eth_header.ether_type));
	printf("ARP FORMAT HARD ADDR: %02X \n", ntohs(request->arp_header.ea_hdr.ar_hrd));
	printf("ARP FORMAT PROTO ADDR: %02X \n", ntohs(request->arp_header.ea_hdr.ar_pro));
	printf("ARP LEN HARD ADDR: %02X \n", request->arp_header.ea_hdr.ar_hln);
	printf("ARP LEN PROTO ADDR: %02X \n", request->arp_header.ea_hdr.ar_pln);
	printf("ARP OP: %02X \n", ntohs(request->arp_header.ea_hdr.ar_op));
	printf("ARP SENDER HARD ADDR: %02X%02X%02X%02X%02X%02X \n", request->arp_header.arp_sha[0],
								request->arp_header.arp_sha[1], request->arp_header.arp_sha[2],
								request->arp_header.arp_sha[3], request->arp_header.arp_sha[4],
								request->arp_header.arp_sha[5]);
	printf("ARP SENDER PROTO ADDR: %02X%02X%02X%02X \n", request->arp_header.arp_spa[0],
						 request->arp_header.arp_spa[1], request->arp_header.arp_spa[2],
						 request->arp_header.arp_spa[3]);
	printf("ARP TARGET HARD ADDR: %02X%02X%02X%02X \n", request->arp_header.arp_tha[0],
						request->arp_header.arp_tha[1], request->arp_header.arp_tha[2],
						request->arp_header.arp_tha[3]);
	printf("ARP TARGET PROTO ADDR: %02X%02X%02X%02X \n\n",request->arp_header.arp_tpa[0],
   	  				   	  request->arp_header.arp_tpa[1], request->arp_header.arp_tpa[2],
					      request->arp_header.arp_tpa[3]);
}





/****************************************************************************************
* print_ETHERTYPE_IP
****************************************************************************************/
void print_ETHERTYPE_IP(struct iicmp reply){
	printf("ETHER DEST: %02X%02X%02X%02X%02X%02X \n", reply.eth_header.ether_dhost[0],
					 reply.eth_header.ether_dhost[1], reply.eth_header.ether_dhost[2],
					 reply.eth_header.ether_dhost[3], reply.eth_header.ether_dhost[4],
					 reply.eth_header.ether_dhost[5]);
	printf("ETHER SRC: %02X%02X%02X%02X%02X%02X \n", reply.eth_header.ether_shost[0],
					reply.eth_header.ether_shost[1], reply.eth_header.ether_shost[2],
					reply.eth_header.ether_shost[3], reply.eth_header.ether_shost[4],
					reply.eth_header.ether_shost[5]);
	printf("ETHER TYPE: %02X \n\n", ntohs(reply.eth_header.ether_type));
	printf("IP IHL: %01X \n", reply.ip_header.ihl);
	printf("IP VERSION: %01X \n", reply.ip_header.version);
	printf("IP TOS: %02X \n", reply.ip_header.tos);
	printf("IP TOT_LEN: %02X \n", ntohs(reply.ip_header.tot_len));
	printf("IP ID: %02X \n", ntohs(reply.ip_header.id));
	printf("IP FRAG_OFF: %02X \n", ntohs(reply.ip_header.frag_off));
	printf("IP TTL: %02X \n", reply.ip_header.ttl);
	printf("IP PROTOCOL: %02X \n", reply.ip_header.protocol);
	printf("IP CHECK: %02X \n", ntohs(reply.ip_header.check));
	printf("IP SADDR: %02X \n", ntohl(reply.ip_header.saddr));
	printf("IP DADDR: %02X \n\n", ntohl(reply.ip_header.daddr));
	printf("ICMP TYPE: %02X \n", reply.icmp_header.type);
	printf("ICMP CODE: %02X \n", reply.icmp_header.code);
	printf("ICMP CHECKSUM: %02X \n\n", ntohs(reply.icmp_header.checksum));
}
