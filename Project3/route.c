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

struct iip{
	struct ether_header eth_header;
	struct iphdr ip_header;
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
struct routing_table *rtable;



/* PROTOTYPES **/
void load_table(char *filename);
void print_ETHERTYPE_ARP(struct aarp *request);
void print_ETHERTYPE_IP(struct iicmp reply);
void clearArray(char *array);
int calculateIcmpChecksum(char *buf, int length);





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
  
  	load_table(argv[1]);

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
		printf("Network  : %s \n", inet_ntoa(*((struct in_addr*)&tempRtable->network)));
		printf("Prefix   : %d \n", tempRtable->prefix);
		printf("Hop      : %s \n", inet_ntoa(*((struct in_addr*)&tempRtable->hop)));
		printf("Interface: %s \n", tempRtable->interface); 
		printf("-----------\n");
		tempRtable = tempRtable->next;
	}



  	printf("Ready to recieve now\n");
  	while(1){
		
		// Loop through all interfaces for packets.
		struct interface * prime_Interface;		
		for(prime_Interface = interfaceList; prime_Interface != NULL; prime_Interface = prime_Interface->next){		

  			char buf[1500];
    		struct sockaddr_ll recvaddr;
    		socklen_t recvaddrlen=sizeof(struct sockaddr_ll);
    		//we can use recv, since the addresses are in the packet, but we
    		//use recvfrom because it gives us an easy way to determine if
    		//this packet is incoming or outgoing (when using ETH_P_ALL, we
    		//see packets in both directions. Only outgoing can be seen when
    		//using a packet socket with some specific protocol)
   		
			int n = recvfrom(prime_Interface->packet_socket, buf, 1500,0,(struct sockaddr*)&recvaddr, 							&recvaddrlen);
 		
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
    		printf("***** Got a %d byte packet *****\n", n);

			printf("Interface Was: %s\n", prime_Interface->name);
	

			// Store the original interface ip as a u_int32
			//u_int32_t interfaceIP = prime_Interface->ip_addrs[0] | 
			//				 	   (prime_Interface->ip_addrs[1] << 8) | 								
			//					   (prime_Interface->ip_addrs[2] << 16) | 
			//					   (prime_Interface->ip_addrs[3] << 24);
			
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// NEW CODE, NOT FINISHED.


			// Recieved ARP.
			if((ntohs(recvaddr.sll_protocol) == ETH_P_ARP)){
				printf("----- Recieved ARP -----\n");

// START: process ARP request.
				

				// The original arp packet sent to us.
				struct aarp *request_ARP;
				request_ARP = ((struct aarp*)&buf);


				// The arp packet we will send.
				struct aarp reply_ARP = *request_ARP;


				//struct aarp *tempArp;
				//tempArp  = ((struct aarp*)&buf);

				// Arp_header op_code was not arp reply
				if (request_ARP->arp_header.ea_hdr.ar_op != ntohs(ARPOP_REPLY)){

					printf("ARP - op = 'REQUEST'\n");
				
					// Store arp_header ip as u_int32
					u_int32_t ip_ARP = request_ARP->arp_header.arp_tpa[3] << 24 | 
									   request_ARP->arp_header.arp_tpa[2] << 16 | 										   
									   request_ARP->arp_header.arp_tpa[1] << 8 | 
									   request_ARP->arp_header.arp_tpa[0];

					// Store interface ip as u_int32
					u_int32_t ip_INT = prime_Interface->ip_addrs[0] | 
									  (prime_Interface->ip_addrs[1] << 8) | 
						    		  (prime_Interface->ip_addrs[2] << 16) | 
						   			  (prime_Interface->ip_addrs[3] << 24);
				 	
					// Interface ip matches arp_header ip, then we want this arp request.
					if(ip_ARP == ip_INT){

						printf("ARP - found interface\n");

						// Change the arp_header op_code to reply.
						reply_ARP.arp_header.ea_hdr.ar_op = htons(ARPOP_REPLY);

						// Swap arp_header sender ip to arp_header target ip and vice versa. 
						memcpy(reply_ARP.arp_header.arp_spa, request_ARP->arp_header.arp_tpa, 4);
						memcpy(reply_ARP.arp_header.arp_tpa, request_ARP->arp_header.arp_spa, 4);

						// Set arp_header target mac to arp_header sender mac.
						memcpy(reply_ARP.arp_header.arp_tha, request_ARP->arp_header.arp_sha, 6);

						// Set arp_header sender mac to interface mac.
						memcpy(reply_ARP.arp_header.arp_sha, prime_Interface->mac_addrs, 6);
						
						// Set eth_header destination mac to eth_header sender mac.
						memcpy(reply_ARP.eth_header.ether_dhost, request_ARP->eth_header.ether_shost, 6);

						// Set eth_header sender mac to interface mac.
						memcpy(reply_ARP.eth_header.ether_shost, prime_Interface->mac_addrs, 6);

						printf("ARP - sending reply\n");

						// Send the ARP reply.
						send(prime_Interface->packet_socket, &reply_ARP, sizeof(reply_ARP), 0);
					}

					// Interface ip doesn't match arp ip, disregard the arp request.
					else {
					
						printf("We dont want this ARP packet sir\n");

					}
				}
				else {
				
					printf("ARP - op = 'OTHER'\n");	

				}

// END: process ARP requst.

			}

			// Recieved IP.
			else if ((ntohs(recvaddr.sll_protocol) == ETH_P_IP)){
				printf("----- Recieved IP -----\n");


				// The original packet sent to us.
				struct iicmp *request_IICMP;
				request_IICMP = ((struct iicmp*)&buf);


				// The packet we will send.
				struct iicmp reply_IICMP = *request_IICMP;

				
				// If protocol is recieve ICMP packets for all local sockets.
				if (request_IICMP->ip_header.protocol == IPPROTO_ICMP) {	
				
					// If the icmp_header type is an icmp echo or reply.
					if (request_IICMP->icmp_header.type == ICMP_ECHO || 
						request_IICMP->icmp_header.type == ICMP_ECHOREPLY) {
             			
						if (request_IICMP->icmp_header.type == ICMP_ECHO)							
							printf("ICMP - Received ICMP ECHO\n");
						else
							printf("ICMP - Received ICMP REPLY\n");

// START: process ICMP echo request.
						

			// START: find interface given ip address.

						printf("ICMP - Scanning interfaces...\n");

						// Create a temp interface pointer for looping.
						struct interface * tmp1_INT;
						tmp1_INT = interfaceList;

						// Temp char array to hold current interface name.
						char   name1A_INT[7];
						char * name1P_INT = NULL;

						// START: Loop - interface list.
						while (tmp1_INT != NULL){

							// Store the interface ip as a u_int32
							u_int32_t ip_INT;
							ip_INT = tmp1_INT->ip_addrs[0] | 
							 	   (tmp1_INT->ip_addrs[1] << 8) | 								
								   (tmp1_INT->ip_addrs[2] << 16) | 
								   (tmp1_INT->ip_addrs[3] << 24);

							//u_int32_t ip_TEMP_IICMP ;
							//memcpy(&ip_TEMP_IICMP, &request_IICMP->ip_header.daddr, 4);

							printf("WHHHHY INT: %X \n", ip_INT);
							printf("WHHHHY REQ: %X \n", request_IICMP->ip_header.daddr);

							
							if (ip_INT == request_IICMP->ip_header.daddr){

								printf("ICMP - Found interface: %s\n", tmp1_INT->name);

								// Copy the temp interface name to i_name, exit the loop.
								memcpy(&name1A_INT, &tmp1_INT->name, 7);

								memcpy(&name1P_INT, &name1A_INT, 7);

								break; 
							}

							// Set the temp interface to the next interface in the list.
							tmp1_INT = tmp1_INT->next;

						}

						// END: Loop - interface list.
			// END: find interface given ip address
					
						// If there was a match in the loop.
						if (name1P_INT != NULL) {

							printf("ICMP - We got mail!!!\n");

			// START: find mac given interface

							// Create a temp interface pointer for looping.
							struct interface * tmp2_INT;
							tmp2_INT = interfaceList;

							u_int8_t mac_INT[6];
					
							// START: Loop - interface list
							while (tmp2_INT != NULL){
			
								// Temp char array to hold current interface name.
								char name2_INT[7];
								memcpy(&name2_INT, tmp2_INT->name, 7);

								if (strcmp(name2_INT, name1P_INT) == 0){

									memcpy(&mac_INT, tmp2_INT->mac_addrs, 6);
									break; 
								}

								tmp2_INT = tmp2_INT->next;
							}
							// END: Loop - interface list.

			// END: find mac given interface

							printf("ICMP - MAC ADDRESS: %X:%X:%X:%X:%X:%X\n", mac_INT[0], mac_INT[1], mac_INT[2],
																	          mac_INT[3], mac_INT[4], mac_INT[5]);

							reply_IICMP.icmp_header.type = ICMP_ECHOREPLY;

							reply_IICMP.icmp_header.checksum = 0;
							//int timeTOlive;

							printf("ICMP - Adjusting time to live\n");

							if (reply_IICMP.ip_header.ttl == 1){
								printf("ICMP - This packets lifeforce has expired\n");

			// START: send ICMP error - ICMP_TIME_EXCEEDED
							
	
								memcpy(reply_IICMP.eth_header.ether_shost, prime_Interface->mac_addrs, 6);
								memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

								reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);

								reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
								memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);
								reply_IICMP.ip_header.check    = 0;
								reply_IICMP.ip_header.frag_off = 0; 
								reply_IICMP.ip_header.ihl      = 5;
								reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
								reply_IICMP.ip_header.tos      = 0;
								reply_IICMP.ip_header.tot_len  = htons(28); 
								reply_IICMP.ip_header.ttl      = 64;
								reply_IICMP.ip_header.version  = 4;
							
								reply_IICMP.icmp_header.type = ICMP_TIME_EXCEEDED;
								reply_IICMP.icmp_header.checksum = 0;
								reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;




printf("FIX ----- ME");
// FIX ME: NEED TO UPDATE CHECKSUM, THEN SEND.

								/*

								int datalength = ntohs(requestIICMP->ip_header.tot_len) - 
											sizeof(requestIICMP->ip_header) - sizeof(requestIICMP->icmp_header);
								unsigned char *data = NULL;
								unsigned char ptr[sizeof(replyIICMP.icmp_header) + datalength];
								memcpy(ptr, &replyIICMP.icmp_header, sizeof(replyIICMP.icmp_header));
								memcpy(ptr + sizeof(replyIICMP.icmp_header), data, datalength);
								replyIICMP.icmp_header.checksum = ip_checksum(&ptr, sizeof(ptr));		
								unsigned char result[sizeof(replyIICMP) + datalength];
								memcpy(result, &replyIICMP, sizeof(replyIICMP));
								memcpy(result + sizeof(replyIICMP), data, datalength);
	



								send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);

								*/


								
			// END: send ICMP error - ICMP_TIME_EXCEEDED

							}
							else {
					

								reply_IICMP.ip_header.ttl = request_IICMP->ip_header.ttl - 1;
								reply_IICMP.ip_header.check = 0;

								reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
								reply_IICMP.ip_header.saddr = request_IICMP->ip_header.daddr;

								memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);
								memcpy(reply_IICMP.eth_header.ether_shost, mac_INT, 6);

								unsigned char *data;
								int datalength = ntohs(request_IICMP->ip_header.tot_len) - 
																	 sizeof(request_IICMP->ip_header) - 
																	 sizeof(request_IICMP->icmp_header);

								if(datalength > 0){
									data = malloc(datalength);
									memcpy(data, buf + sizeof(struct iicmp), datalength);
								}

				
								unsigned char ptr2[sizeof(reply_IICMP.icmp_header) + datalength];
								memcpy(ptr2, &reply_IICMP.icmp_header, sizeof(reply_IICMP.icmp_header));
								memcpy(ptr2 + sizeof(reply_IICMP.icmp_header), data, datalength);

								reply_IICMP.icmp_header.checksum = ip_checksum(&ptr2, sizeof(ptr2));
			
								unsigned char result[sizeof(reply_IICMP) + datalength];
					
								memcpy(result, &reply_IICMP, sizeof(reply_IICMP));
								memcpy(result + sizeof(reply_IICMP), data, datalength);


								printf("ICMP - Sending packet\n");
								printf("Sending on interface: %s\n", prime_Interface->name);

								send(prime_Interface->packet_socket, &result, sizeof(result), 0);

							}
						}

						// Did not find an interface name.
						else {

							printf("Whose packet is this???\n");

							struct interface * prize_Interface = NULL;
							u_int32_t ip_HOP = 0;

			// START: find interface from ip

							printf("searching page table...\n");
							
							int foundMatch = 0;

							struct interface * tmp_INT;
							tmp_INT = interfaceList;

							while (tmp_INT != NULL){

								struct routing_table * tmp_TBL;
								tmp_TBL = rtable;

								while (tmp_TBL != NULL){

									if (tmp_TBL->network << (32 - tmp_TBL->prefix) == 
													request_IICMP->ip_header.daddr << (32 - tmp_TBL->prefix)){
										
										if (strcmp(tmp_TBL->interface, tmp_INT->name) == 0){
										
											foundMatch = 1;
											ip_HOP = tmp_TBL->hop;
											prize_Interface = tmp_INT;
											printf("Found interface: %s\n", prize_Interface->name);
											u_int8_t ip_print[4];
											memcpy(&ip_print, &ip_HOP, 4);
											printf("Found hop: %X.%X.%X.%X\n", ip_print[0],ip_print[1],ip_print[2],ip_print[3]);
											break;
										}
									}

									tmp_TBL = tmp_TBL->next;									

								}
								if (foundMatch){

									break;
								}
								tmp_INT = tmp_INT->next;
							}

			// END: find interface from ip.
							
							
							if (prize_Interface != NULL){

								u_int8_t * mac_HOST;

			// START: send ARP request.

								if (ip_HOP == 0 || ip_HOP == -1){
								
									ip_HOP = request_IICMP->ip_header.daddr;
								}
								

								u_int8_t ip_print[4];
								memcpy(&ip_print, &ip_HOP, 4);
								printf("New hop: %X.%X.%X.%X\n", ip_print[0],ip_print[1],ip_print[2],ip_print[3]);


								//struct aarp * temp_ARP = malloc(sizeof(struct aarp));


								struct aarp *temp_ARP;
								temp_ARP = malloc(sizeof(struct aarp));
								//req_ARP = ((struct aarp*)&buf);
								//struct aarp temp_ARP = *req_ARP;

								temp_ARP->eth_header.ether_type = htons(ETHERTYPE_ARP);


								char broadcast_255[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast_0[6]   = {0,0,0,0,0,0};

								memcpy(&temp_ARP->eth_header.ether_dhost, broadcast_255, 6);

								memcpy(&temp_ARP->eth_header.ether_shost, prize_Interface->mac_addrs, 6);

								memcpy(temp_ARP->arp_header.arp_tha, &broadcast_0, 6);
								memcpy(temp_ARP->arp_header.arp_sha, prize_Interface->mac_addrs, 6);
								
								temp_ARP->arp_header.arp_tpa[3] = (uint8_t) (ip_HOP >> 24);
								temp_ARP->arp_header.arp_tpa[2] = (uint8_t) (ip_HOP >> 16);
								temp_ARP->arp_header.arp_tpa[1] = (uint8_t) (ip_HOP >> 8);
								temp_ARP->arp_header.arp_tpa[0] = (uint8_t) (ip_HOP);

								u_int32_t temp_ip_INT = prize_Interface->ip_addrs[0] | 
									  		    	   (prize_Interface->ip_addrs[1] << 8) | 
						    		  		           (prize_Interface->ip_addrs[2] << 16) | 
												       (prize_Interface->ip_addrs[3] << 24);

								temp_ARP->arp_header.arp_spa[3] = (uint8_t) (temp_ip_INT >> 24);
								temp_ARP->arp_header.arp_spa[2] = (uint8_t) (temp_ip_INT >> 16);
								temp_ARP->arp_header.arp_spa[1] = (uint8_t) (temp_ip_INT >> 8);
								temp_ARP->arp_header.arp_spa[0] = (uint8_t) (temp_ip_INT);

								temp_ARP->arp_header.ea_hdr.ar_hln = 6;
								temp_ARP->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								temp_ARP->arp_header.ea_hdr.ar_pln = 4;
								temp_ARP->arp_header.ea_hdr.ar_pro = htons(ETH_P_IP);
								temp_ARP->arp_header.ea_hdr.ar_op  = htons(ARPOP_REQUEST);

								
								printf("Sending ARP request\n");
					

								send(prize_Interface->packet_socket, temp_ARP, sizeof(struct aarp), 0);
									
								
								struct sockaddr_ll temp_Recv;

  								struct timeval tv2;
  								tv2.tv_sec = 0;
  								tv2.tv_usec = 1000 * 20;

  								if (setsockopt(prize_Interface->packet_socket, SOL_SOCKET, SO_RCVTIMEO,&tv2,
																					sizeof(tv2)) < 0) {
    								perror("Error");
								}

								socklen_t temp_Recvlen = sizeof(struct sockaddr_ll);
								char temp_Buf[1500];

  								int n2 = recvfrom(prize_Interface->packet_socket, temp_Buf, 1500, 0, 
																(struct sockaddr*)&temp_Recv, &temp_Recvlen);
 												
			
  								if (n2 < 1){
    								mac_HOST = NULL;
								}
								else {

									tv2.tv_sec = 0;
								    tv2.tv_usec = 1000;

									if (setsockopt(prize_Interface->packet_socket, SOL_SOCKET,SO_RCVTIMEO,
																				&tv2,sizeof(tv2)) < 0) {
										perror("Error");
									}

									struct aarp new_aarp;

									memcpy(&new_aarp, temp_Buf, sizeof(struct aarp));
									
									mac_HOST = malloc(sizeof(u_int8_t)*6);
									memcpy(mac_HOST, new_aarp.arp_header.arp_sha, 6);

								}

			// END: send ARP request.

								
								if (mac_HOST == NULL) {
					
									printf("No MAC received from ARP request\n");
									printf("Sending ICMP error\n");

			// START: send ICMP error - ICMP_DEST_UNREACH.

									memcpy(reply_IICMP.eth_header.ether_shost, prime_Interface->mac_addrs, 6);
									memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

									reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);

									reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
									memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);
									reply_IICMP.ip_header.check    = 0;
									reply_IICMP.ip_header.frag_off = 0; 
									reply_IICMP.ip_header.ihl      = 5;
									reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
									reply_IICMP.ip_header.tos      = 0;
									reply_IICMP.ip_header.tot_len  = htons(28); 
									reply_IICMP.ip_header.ttl      = 64;
									reply_IICMP.ip_header.version  = 4;
							
									reply_IICMP.icmp_header.type = ICMP_HOST_UNREACH;
									reply_IICMP.icmp_header.checksum = 0;
									reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;


// FIX ME: CALC CHECKSUM AND SEND. 


									send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);

			// END: send ICMP error - ICMP_DEST_UNREACH.
								}
								else {

									printf("MAC from ARP request: %02X:%02X:%02X:%02X:%02X:%02X\n", 
																	mac_HOST[0], mac_HOST[1], mac_HOST[2], 
																	mac_HOST[3], mac_HOST[4], mac_HOST[5]);


									reply_IICMP.eth_header.ether_type = htons(ETH_P_IP);

									memcpy(reply_IICMP.eth_header.ether_dhost, mac_HOST, 6);
									memcpy(reply_IICMP.eth_header.ether_shost, prize_Interface->mac_addrs, 6);

									reply_IICMP.icmp_header.checksum = 0;
									
									if (request_IICMP->ip_header.ttl == 1){

										printf("The packet died\n");
										// ICMP error - packet died in our arms.

									}
									else {


										reply_IICMP.ip_header.ttl = request_IICMP->ip_header.ttl - 1;
										reply_IICMP.ip_header.check = 0;

			
										unsigned char *data;
										int datalength = ntohs(request_IICMP->ip_header.tot_len) - 
																	 sizeof(request_IICMP->ip_header) - 
																	 sizeof(request_IICMP->icmp_header);

										if(datalength > 0){
											data = malloc(datalength);
											memcpy(data, buf + sizeof(struct iicmp), datalength);
										}

										unsigned char ptr2[sizeof(reply_IICMP.icmp_header) + datalength];
										memcpy(ptr2, &reply_IICMP.icmp_header, sizeof(reply_IICMP.icmp_header));
										memcpy(ptr2 + sizeof(reply_IICMP.icmp_header), data, datalength);

										reply_IICMP.icmp_header.checksum = ip_checksum(&ptr2, sizeof(ptr2));
									
										unsigned char result[sizeof(reply_IICMP) + datalength];
										
										memcpy(result, &reply_IICMP, sizeof(reply_IICMP));
										memcpy(result + sizeof(reply_IICMP), data, datalength);

										printf("Sending ICMP reply\n");

										send(prize_Interface->packet_socket, &result, sizeof(result), 0);
	
									}
								}
							}
							// No address was found in table, send an error.
							else {

								printf("Not my neighbor, sending ICMP error\n");

			// START: send ICMP error - ICMP_DEST_UNREACH.



								memcpy(reply_IICMP.eth_header.ether_shost, prime_Interface->mac_addrs, 6);
								memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

								reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);

								reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
								memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);
								reply_IICMP.ip_header.check    = 0;
								reply_IICMP.ip_header.frag_off = 0; 
								reply_IICMP.ip_header.ihl      = 5;
								reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
								reply_IICMP.ip_header.tos      = 0;
								reply_IICMP.ip_header.tot_len  = htons(28); 
								reply_IICMP.ip_header.ttl      = 64;
								reply_IICMP.ip_header.version  = 4;
						
								reply_IICMP.icmp_header.type = ICMP_HOST_UNREACH;
								reply_IICMP.icmp_header.checksum = 0;
								reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;


// FIX ME: CALC CHECKSUM AND SEND.


								//send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);


			// END: send ICMP error - ICMP_DEST_UNREACH.

							}
						}

// END: process ICMP echo request.

            		} 
					else {
              			printf("Forwarding Packet\n");

// START: forward packet.


			// START: find interface with the correct ip.

						char *i_name;
						i_name = NULL;

						struct interface * iface1;

						for(iface1 = interfaceList; iface1 != NULL; iface1 = iface1->next) {

							u_int32_t temp_ip;
							memcpy(&temp_ip, iface1->ip_addrs, 4);
							if(temp_ip == request_IICMP->ip_header.daddr){

								printf("interface name: %s\n", iface1->name);
								memcpy(i_name, iface1->name, 7);
							  	break;
							}
						}

			// END: find interface with the correct ip.


						if(i_name != NULL) {

						 	printf("This is for us!\n");

							u_int8_t tmp_MAC[6];

		
			// START: find mac with interface.

							struct interface * iface2;
							for(iface2 = interfaceList; iface2 != NULL; iface2 = iface2->next) {

								if(strcmp(iface2->name, i_name)){

								  	memcpy(tmp_MAC, iface2->mac_addrs, 6);
								  	break;
								}
							}

			// END: find mac with interface.
							

 							memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);
							memcpy(reply_IICMP.eth_header.ether_shost, tmp_MAC, 6);
							
							send(prime_Interface->packet_socket, &reply_IICMP, sizeof(struct iicmp), 0);

  
						}

						else {
						  	//ICMP echo is not for us, figure out the correct interfaces to send to
						  	printf("This isn't for us\n");
						  	//Check that address in on our routing table


						  	


			// START: find interface from ip address.

							struct interface * forwardInterface = NULL;
							u_int32_t forward_ip = 0;

							int breakLoop = 0;
							
							struct interface * iface2;

						  	for(iface2 = interfaceList; iface2 != NULL; iface2 = iface2->next) {

								struct routing_table * temptable;

								for(temptable = rtable; temptable!=NULL; temptable = temptable->next) {

							  		if (temptable->network << (32 - temptable->prefix) == request_IICMP->ip_header.daddr << (32 - temptable->prefix)) {
									
										forwardInterface = iface2;
										forward_ip = temptable->network;
										breakLoop = 1;
										break;
							  		}
								}
								if (breakLoop){
									break;
								}
						  	}

			// END: find interface from ip address



						  	if(forwardInterface != NULL){

								u_int8_t * forward_mac = NULL;
								

								if(forward_ip == 0) {
								  	
									forward_ip = request_IICMP->ip_header.daddr;
								
								}

							
// START: send arp request.



// There is code above that we can use for this.



// END: send arp request.
							


								if(forward_mac == NULL){

								  	//send ICMP error
								  	printf("ARP returned no MAC\n");


								}
								else
								{
								  	printf("ARP returned MAC:\n");
								  	printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
									forward_mac[0],forward_mac[1],
									forward_mac[2],forward_mac[3],
									forward_mac[4],forward_mac[5]);

								  	
									memcpy(reply_IICMP.eth_header.ether_dhost, forward_mac, 6);
									memcpy(reply_IICMP.eth_header.ether_shost, forwardInterface->mac_addrs, 6);

									reply_IICMP.eth_header.ether_type = htons(ETH_P_IP);

								  	send(forwardInterface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);
								}

						  	}
						  	else{

								printf("Address not found in table, sending error\n");
						  	}
						}
					






// END: forward packet.~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
					}
				}
				else {
            		printf("Recieved packet (not ARP or ICMP), Forwarding\n");

// START: forward packet.
            		
// There is code above that can be reused for this.

// END: forward packet.

				}
			}
			else {
				printf("Dropping this packet because why not?\n");
			}
		}
	}
	return 0;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++






/*





struct aarp *tempArp;
			tempArp  = ((struct aarp*)&buf);
			struct iicmp *tempIcmp;
			tempIcmp = ((struct iicmp*)&buf);




				struct aarp *request;
		
				request = ((struct aarp*)&buf);
				
				// Print the request contents.	
				print_ETHERTYPE_ARP(request);

				// Create reply structure.
				struct aarp reply = *request;
	
				// IS THIS FOR US?
				if (request->arp_header.arp_spa == tempInterface->ip_addrs){
					printf("YOU'VE GOT MAIL\n");
				
				}



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
				printf("Sent ARP reply \n");

			}else if((ntohs(recvaddr.sll_protocol) == ETH_P_IP) && n > -1 && tempIcmp->icmp_header.type == 										ICMP_ECHO && tempIcmp->ip_header.daddr == interfaceIP){
				
		
				char buf2[1500];
				memcpy(buf2, buf, sizeof(buf));
		
				struct iicmp request2;
				unsigned char *data;
				request2 = *((struct iicmp*)&buf2);
				int datalength = ntohs(request2.ip_header.tot_len) - sizeof(request2.ip_header) - 
																	 sizeof(request2.icmp_header);
//fdbg
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
//dsd				memcpy(&reply.eth_header.ether_dhost, request2.eth_header.ether_shost, ETH_ALEN);
				memcpy(&reply.ip_header.daddr, tmp3, 4);
				memcpy(&reply.ip_header.saddr, tmp4, 4);
				reply.icmp_header.type = ICMP_ECHOREPLY;
				reply.icmp_header.checksum = 0;
				unsigned char ptr[sizeof(reply.icmp_header) + datalength];
				memcpy(ptr, &reply.icmp_header, sizeof(reply.icmp_header));
				memcpy(ptr + sizeof(reply.icmp_header), data, datalength);
				reply.icmp_header.checksum = ip_checksum(&ptr, sizeof(ptr));
		
				// Print the reply contents.
				//print_ETHERTYPE_IP(reply);		
	
				unsigned char result[sizeof(reply) + datalength];
				memcpy(result, &reply, sizeof(reply));
				memcpy(result + sizeof(reply), data, datalength);
		
				send(tempInterface->packet_socket, &result, sizeof(result), 0);
			}else if ((ntohs(recvaddr.sll_protocol) == ETH_P_IP) && n > -1 && 
						ntohs(tempIcmp->eth_header.ether_type) == ETHERTYPE_IP){
				struct iip *iip;
				iip = ((struct iip*)&buf);
				
				struct aarp *request = malloc(sizeof(struct aarp));
				request->eth_header.ether_type = htons(ETHERTYPE_ARP);
			
				//figure out next hop IP and interface
				struct routing_table *tempRtable = rtable;
				int skip = 0;
				while(tempRtable != NULL){	
					char chars[4];
					memcpy(chars, &tempRtable->network, 4);
					if((tempRtable->prefix == 24 && 
						(tempRtable->network >> 8) | 
						(iip->ip_header.daddr >> 8)) == 0){	
						struct interface *iList = interfaceList;
						while(iList != NULL){
							if(strcmp(iList->name, tempRtable->interface) == 0){									
								//printf("NEXT HOP: %s \n", inet_ntoa(*((struct in_addr*)&tempRtable->hop)));		
								memcpy(request->eth_header.ether_shost, iList->mac_addrs, 6);
								char broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast2[6] = {0,0,0,0,0,0};
								memcpy(request->eth_header.ether_dhost, &broadcast, 6);
								request->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								request->arp_header.ea_hdr.ar_pro = htons(0x800);
								request->arp_header.ea_hdr.ar_hln = sizeof(iList->mac_addrs); //6
								request->arp_header.ea_hdr.ar_pln = sizeof(iList->ip_addrs);
								request->arp_header.ea_hdr.ar_op = htons(ARPOP_REQUEST);
								memcpy(request->arp_header.arp_sha, iList->mac_addrs, 6); //NOTE
								memcpy(request->arp_header.arp_spa, iList->ip_addrs, 4);
								memcpy(request->arp_header.arp_tha, &broadcast2, 6);
								if(tempRtable->hop < 0xffffffff){
									memcpy(request->arp_header.arp_tpa, &tempRtable->hop, 4);
								}else{						
									memcpy(request->arp_header.arp_tpa,&iip->ip_header.daddr,4);						
								}		
								printf("IPADDR: %s", inet_ntoa(*((struct in_addr*) &iip->ip_header.daddr)));			
								//print_ETHERTYPE_ARP(request);
								send(iList->packet_socket, request, sizeof(struct aarp), 0);	
							}
							iList = iList->next;
						}	
						skip = 1;
						break;					
					}
					tempRtable = tempRtable->next;
				}
				if(skip == 0){
					tempRtable = rtable;
					while(tempRtable != NULL){	
					char chars[4];
					memcpy(chars, &tempRtable->network, 4);
					if((tempRtable->prefix == 16 && 
						(tempRtable->network >> 16) | 
						(iip->ip_header.daddr >> 16)) == 0){	
						struct interface *iList = interfaceList;
						while(iList != NULL){
							if(strcmp(iList->name, tempRtable->interface) == 0){									
								//printf("NEXT HOP: %s \n", inet_ntoa(*((struct in_addr*)&tempRtable->hop)));		
								memcpy(request->eth_header.ether_shost, iList->mac_addrs, 6);
								char broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast2[6] = {0,0,0,0,0,0};
								memcpy(request->eth_header.ether_dhost, &broadcast, 6);
								request->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								request->arp_header.ea_hdr.ar_pro = htons(0x800);
								send(iList->packet_socket, request, sizeof(struct aarp), 0);	
								request->arp_header.ea_hdr.ar_hln = sizeof(iList->mac_addrs); //6
								request->arp_header.ea_hdr.ar_pln = sizeof(iList->ip_addrs);
								request->arp_header.ea_hdr.ar_op = htons(ARPOP_REQUEST);
								memcpy(request->arp_header.arp_sha, iList->mac_addrs, 6); //NOTE
								memcpy(request->arp_header.arp_spa, iList->ip_addrs, 4);
								memcpy(request->arp_header.arp_tha, &broadcast2, 6);
								if(tempRtable->hop < 0xffffffff){
										memcpy(request->arp_header.arp_tpa, &tempRtable->hop, 4);
									}else{						
										memcpy(request->arp_header.arp_tpa,&iip->ip_header.daddr,4);						
									}		
								printf("IPADDR: %s", inet_ntoa(*((struct in_addr*) &iip->ip_header.daddr)));			
								//print_ETHERTYPE_ARP(request);
								send(iList->packet_socket, request, sizeof(struct aarp), 0);	
							}
							iList = iList->next;
						}	
						skip = 1;
						break;					
					}
					tempRtable = tempRtable->next;
				}
				}			
						
				//wait for ARP response (timeout)
				
				
				//print
			}else if((ntohs(recvaddr.sll_protocol) == ETH_P_ARP) && n > -1 && ntohs(tempArp->arp_header.ea_hdr.ar_op) == ARPOP_REPLY){
				struct aarp *request;
		
				request = ((struct aarp*)&buf);
				
				
				printf("Next Hop MAC: %02X:%02X:%02X:%02X:%02X:%02X \n", request->arp_header.arp_sha[0],
								request->arp_header.arp_sha[1], request->arp_header.arp_sha[2],
								request->arp_header.arp_sha[3], request->arp_header.arp_sha[4],
								request->arp_header.arp_sha[5]);
				printf("Next Hop IP: %u.%u.%u.%u \n", request->arp_header.arp_spa[0],
						 request->arp_header.arp_spa[1], request->arp_header.arp_spa[2],
						 request->arp_header.arp_spa[3]);
			
			}
		}
  	}
  	// Exit main
  	return 0;
}



**/

/****************************************************************************************
* LOAD ROUTING TABLE  ---- FIX ME ---- 
****************************************************************************************/
void load_table(char *filename){
		
	// Open the file.
	FILE *fp = fopen(filename, "r");  	
  	if (fp == NULL){
    	printf("Could Not Open File");
    	exit(1);
	}
	
	// Routing table items.
	struct routing_table *tempRtable, *prevRT;
	tempRtable = NULL;
	int newRtable = 1;

	// Local Variables.
	int caseNum = 0;
	int i;
	char item[9];
	int index = 0;
	char c;

	// Loop through the file until EOF.
	while ((c = fgetc(fp)) != EOF){

		// Setup a routing table to hold values.
		if(newRtable == 1){		
			for(tempRtable = rtable; tempRtable != NULL; tempRtable = tempRtable->next){
				prevRT = tempRtable;
			}
			tempRtable = malloc(sizeof(struct routing_table));
			if(rtable == NULL){
				rtable = tempRtable;
			}
			else {
				prevRT->next = tempRtable;
			}
			newRtable = 0;
		}

		// Add the char read to the array.
		item[index++] = c;

		// CASEs for what the array is storing.
		if (c == '/'){ // Network

			printf("NEW NETWORK\n");

			item[--index] = '\0';
			tempRtable->network = (u_int32_t)inet_addr(item);			
			
			for(i = 0; i < 9; i++){
				item[i] = '\0';
			}
			index = 0;
			caseNum++;
		}
		else if (c == ' '){

			if(caseNum == 1){ // Prefix
				printf("NEW PREFIX\n");
				
				item[--index] = '\0';
				tempRtable->prefix = atoi(item);
				
				for(i = 0; i < 9; i++){
					item[i] = '\0';
				}
				index = 0;
				caseNum++;
			}
			else if (caseNum == 2){ // Hop

				printf("NEW HOP\n");
				item[--index] = '\0';
				
				if(item[--index] == '-')
					tempRtable->hop = -1;
				else 
					tempRtable->hop = (u_int32_t)inet_addr(item);
				
				for(i = 0; i < 9; i++){
					item[i] = '\0';
				}
				index = 0;
				caseNum++;
			}
		}
		else if (c == '\n' || c == EOF){ // Interface

			printf("NEW LINE\n");

			item[--index] = '\0';
			memcpy(&tempRtable->interface, item, 8);
			
			tempRtable->next = NULL;

			newRtable = 1;
					
			for(i = 0; i < 9; i++){
				item[i] = '\0';
			}
			index = 0;
			caseNum = 0;
		}
	}
	// Close the file.
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

int calculateIcmpChecksum(char *buf, int length){

  int lengthToIcmp = sizeof(struct ether_header)+sizeof(struct iphdr);
  int icmpDataLength = length - (sizeof(struct ether_header)+sizeof(struct iphdr)+sizeof(struct icmphdr)+8);
  int icmpLength = (sizeof(struct icmphdr) + 8 + icmpDataLength);

  struct icmphdr icmpHeader;
  memcpy(&icmpHeader,&buf+lengthToIcmp,sizeof(struct icmphdr));

  unsigned int checksum = 0;
  int i;
  for(i = 0; i < icmpLength; i+=2) {
    checksum += (uint32_t) ((uint8_t) buf[lengthToIcmp+i] << 8 | (uint8_t) buf[lengthToIcmp+i+1]);
  }
  checksum = (checksum >> 16) + (checksum & 0xffff);
  return (uint16_t) ~checksum;
}


