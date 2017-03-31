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
   		
			int n = recvfrom(tempInterface->packet_socket, buf, 1500,0,(struct sockaddr*)&recvaddr, 							&recvaddrlen);
 		
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

			// Store the original interface ip as a u_int32
			u_int32_t interfaceIP = tempInterface->ip_addrs[0] | (tempInterface->ip_addrs[1] << 8) | 								(tempInterface->ip_addrs[2] << 16) | (tempInterface->ip_addrs[3] << 24);
			
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// NEW CODE, NOT FINISHED.


			// Recieved ARP.
			if((ntohs(recvaddr.sll_protocol) == ETH_P_ARP)){
				printf("----- Recieved ARP -----\n");

// START: process ARP request.
				

				// The original arp packet sent to us.
				struct aarp *requestARP;
				requestARP = ((struct aarp*)&buf);


				// The arp packet we will send.
				struct aarp replyARP = *requestARP;


				//struct aarp *tempArp;
				//tempArp  = ((struct aarp*)&buf);

				// Arp_header op_code was not arp reply
				if (requestARP->arp_header.ea_hdr.ar_op != ntohs(ARPOP_REPLY)){

					printf("ARP - op = 'REQUEST'\n");
				
					// Store arp_header ip as u_int32
					u_int32_t ipAddr = requestARP->arp_header.arp_tpa[3] << 24 | 
									   requestARP->arp_header.arp_tpa[2] << 16 | 										   requestARP->arp_header.arp_tpa[1] << 8 | 
									   requestARP->arp_header.arp_tpa[0];

					// Store interface ip as u_int32
					u_int32_t i_ip = tempInterface->ip_addrs[0] | 
									(tempInterface->ip_addrs[1] << 8) | 
						    		(tempInterface->ip_addrs[2] << 16) | 
						   			(tempInterface->ip_addrs[3] << 24);
				 	
					// Interface ip matches arp_header ip, then we want this arp request.
					if(i_ip == ipAddr){

						printf("ARP - found interface\n");

						// Change the arp_header op_code to reply.
						replyARP.arp_header.ea_hdr.ar_op = htons(ARPOP_REPLY);

						// Swap arp_header sender ip to arp_header target ip and vice versa. 
						memcpy(replyARP.arp_header.arp_spa, requestARP->arp_header.arp_tpa, 4);
						memcpy(replyARP.arp_header.arp_tpa, requestARP->arp_header.arp_spa, 4);

						// Set arp_header target mac to arp_header sender mac.
						memcpy(replyARP.arp_header.arp_tha, requestARP->arp_header.arp_sha, 6);

						// Set arp_header sender mac to interface mac.
						memcpy(replyARP.arp_header.arp_sha, tempInterface->mac_addrs, 6);
						
						// Set eth_header destination mac to eth_header sender mac.
						memcpy(replyARP.eth_header.ether_dhost, requestARP->eth_header.ether_shost, 6);

						// Set eth_header sender mac to interface mac.
						memcpy(replyARP.eth_header.ether_shost, tempInterface->mac_addrs, 6);

						printf("ARP - sending reply\n");

						// Send the ARP reply.
						send(tempInterface->packet_socket, &replyARP, sizeof(struct aarp), 0);
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
				struct iicmp *requestIICMP;
				requestIICMP = ((struct iicmp*)&buf);


				// The packet we will send.
				struct iicmp replyIICMP = *requestIICMP;


				struct iicmp *tempIcmp;
				tempIcmp = NULL;
				//tempIcmp = ((struct iicmp*)&buf);


				
				// If protocol is recieve ICMP packets for all local sockets.
				if (requestIICMP->ip_header.protocol == IPPROTO_ICMP) {	
				
					// If the icmp_header type is an icmp echo or reply.
					if (requestIICMP->icmp_header.type == ICMP_ECHO || 
						requestIICMP->icmp_header.type == ICMP_ECHOREPLY) {
             			
						if (requestIICMP->icmp_header.type == ICMP_ECHO)							
							printf("Received ICMP ECHO\n");
						else
							printf("Received ICMP REPLY\n");

// START: process ICMP echo request.
						
						printf("Searching for interface...\n");

						struct interface *tmpInterface;
						tmpInterface = interfaceList;
						char *i_name = NULL;

						// START: Loop - interface list.
						while (tmpInterface != NULL){

							// Store the interface ip as a u_int32
							u_int32_t i_ip;
							memcpy(&i_ip, tmpInterface->ip_addrs, 4);

							// If the temp interface ip matchs the original interface ip.
							if (i_ip & interfaceIP){

								printf("Found interface: %s\n", tmpInterface->name);

								// Copy the temp interface name to i_name, exit the loop.
								memcpy(&i_name, tmpInterface->name, 7);

								break; 
							}

							// Set the temp interface to the next interface in the list.

							tmpInterface = tmpInterface->next;
						}
						// END: Loop - interface list.
					
						// If there was a match in the loop.
						if (i_name != NULL) {

							printf("We got mail!!!\n");
printf("FIX --- ME\n");
							struct interface *tmpInterface;
							tmpInterface = interfaceList;
							u_int8_t i_mac[6];
printf("FIX --- ME\n");						
							while (tmpInterface != NULL){
							
								if (strcmp(tmpInterface->name, i_name)){

									memcpy(&i_mac, tmpInterface->mac_addrs, 6);
									break; 
								}
								tmpInterface = tmpInterface->next;
							}
printf("FIX --- ME\n");
							printf("MAC ADDRESS: %X:%X:%X:%X:%X:%X", i_mac[0], i_mac[1], i_mac[2],
																	 i_mac[3], i_mac[4], i_mac[5]);

							replyIICMP.icmp_header.type = ICMP_ECHOREPLY;

							replyIICMP.icmp_header.checksum = 0;
							//int timeTOlive;

							printf("Adjusting time to live");
printf("FIX --- ME\n");
							if (tempIcmp->ip_header.ttl == 1){
								printf("This packets lifeforce has expired");

			// START: send ICMP error - ICMP_TIME_EXCEEDED
printf("FIX --- ME\n");								
								u_int8_t tempAddr[4];
	
								memcpy(&tempAddr, tempIcmp->eth_header.ether_shost, sizeof(tempAddr));
								memcpy(tempIcmp->eth_header.ether_shost, tmpInterface->mac_addrs, 
																				   sizeof(tempAddr));
								memcpy(tempIcmp->eth_header.ether_dhost, tempAddr, sizeof(tempAddr));

								tempIcmp->eth_header.ether_type = htons(ETHERTYPE_IP);

								tempIcmp->ip_header.daddr    = tempIcmp->ip_header.saddr;
								memcpy(&tempIcmp->ip_header.saddr, &tmpInterface->ip_addrs, 
															 sizeof(tmpInterface->ip_addrs));
								tempIcmp->ip_header.check    = 0;
								tempIcmp->ip_header.frag_off = 0; 
								tempIcmp->ip_header.ihl      = 5;
								tempIcmp->ip_header.protocol = IPPROTO_ICMP; 
								tempIcmp->ip_header.tos      = 0;
								tempIcmp->ip_header.tot_len  = htons(28); 
								tempIcmp->ip_header.ttl      = 64;
								tempIcmp->ip_header.version  = 4;
								
								tempIcmp->icmp_header.type = ICMP_TIME_EXCEEDED;
								tempIcmp->icmp_header.checksum = 0;
								tempIcmp->icmp_header.code = ICMP_HOST_UNREACH;

								memcpy(buf, &tempIcmp, sizeof(struct iicmp));
								
								tempIcmp->icmp_header.checksum = ip_checksum(buf, sizeof(buf));
								
								memcpy(buf, &tempIcmp, sizeof(struct iicmp));

								send(tmpInterface->packet_socket, buf, sizeof(buf), 0);

			// END: send ICMP error - ICMP_TIME_EXCEEDED

							}
							else {
					
								tempIcmp->ip_header.ttl = tempIcmp->ip_header.ttl - 1;
								tempIcmp->ip_header.check = 0;
								memcpy(buf, &tempIcmp, sizeof(struct iicmp));
								tempIcmp->ip_header.check = ip_checksum(buf, sizeof(buf));
								tempIcmp->icmp_header.checksum = ip_checksum(buf, sizeof(buf));
								
								u_int32_t tempAddr;
								tempAddr = tempIcmp->ip_header.daddr;
								tempIcmp->ip_header.daddr = tempIcmp->ip_header.saddr;
								tempIcmp->ip_header.saddr = tempAddr;

								memcpy(tempIcmp->eth_header.ether_dhost, tempIcmp->eth_header.ether_shost, 
																  sizeof(tempIcmp->eth_header.ether_dhost));
								memcpy(tempIcmp->eth_header.ether_shost, i_mac, 
																  sizeof(tempIcmp->eth_header.ether_shost));

								memcpy(buf, &tempIcmp, sizeof(struct iicmp));

								send(tmpInterface->packet_socket, buf, sizeof(buf), 0);

							}
						}
						else {

							printf("Whose packet is this???");

							int foundMatch = 0;
							u_int32_t ip_addr;

							struct interface *tmpInterface;
							tmpInterface = interfaceList;
							while (tmpInterface != NULL){

								struct routing_table *tmpTable;
								tmpTable = rtable;
								while (tmpTable != NULL){

									if (tmpTable->network << (32 - tmpTable->prefix) == 
													tempIcmp->ip_header.daddr << (32 - tmpTable->prefix)){

										
										foundMatch = 1;
										ip_addr = tmpTable->hop;
										break;
									}
								}
								if (foundMatch){

									break;
								}
								ip_addr = 0;
							}
							
							
							if (tmpInterface != NULL){

			// START: send ARP request.

								if (ip_addr == 0){
								
									ip_addr = tempIcmp->ip_header.daddr;
								}
								
								// Send ARP request.

								struct aarp *tempARP;
								tempARP = NULL;

								char broadcast255[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast0[6]   = {0,0,0,0,0,0};

								memcpy(tempARP->eth_header.ether_dhost, &broadcast255,
															sizeof(tempIcmp->eth_header.ether_dhost));

								memcpy(tempARP->arp_header.arp_tha, &broadcast0,
															sizeof(tempIcmp->eth_header.ether_dhost));

								memcpy(tempARP->arp_header.arp_sha, tmpInterface->mac_addrs, 6);
								
								tempARP->arp_header.arp_tpa[3] = (uint8_t) (ip_addr >> 24);
								tempARP->arp_header.arp_tpa[2] = (uint8_t) (ip_addr >> 16);
								tempARP->arp_header.arp_tpa[1] = (uint8_t) (ip_addr >> 8);
								tempARP->arp_header.arp_tpa[0] = (uint8_t) (ip_addr);
								
								u_int32_t i_ip = tmpInterface->ip_addrs[0] | 
									  		    (tmpInterface->ip_addrs[1] << 8) | 
						    		  		    (tmpInterface->ip_addrs[2] << 16) | 
						    		  		    (tmpInterface->ip_addrs[3] << 24);

								tempARP->arp_header.arp_spa[3] = (uint8_t) (i_ip >> 24);
								tempARP->arp_header.arp_spa[2] = (uint8_t) (i_ip >> 16);
								tempARP->arp_header.arp_spa[1] = (uint8_t) (i_ip >> 8);
								tempARP->arp_header.arp_spa[0] = (uint8_t) (i_ip);

								tempARP->arp_header.ea_hdr.ar_hln = 6;
								tempARP->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								tempARP->arp_header.ea_hdr.ar_pln = 4;
								tempARP->arp_header.ea_hdr.ar_pro = htons(ETH_P_IP);
								tempARP->arp_header.ea_hdr.ar_op  = htons(ARPOP_REQUEST);
					
								char tempBuf[1500];
								memcpy(tempBuf, tempARP, sizeof(tempBuf));

								send(tmpInterface->packet_socket, tempBuf, sizeof(struct aarp), 0);
									
								
								struct sockaddr_ll tmpRecv;

  								struct timeval tv;
  								tv.tv_sec = 0;
  								tv.tv_usec = 1000 * 20;
  								if (setsockopt(tmpInterface->packet_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,
																					sizeof(tv)) < 0) {
    								perror("Error");
								}

								socklen_t tmpRecvlen = sizeof(struct sockaddr_ll);
  								int n = recvfrom(tmpInterface->packet_socket, tempBuf, 1500, 0, 
																(struct sockaddr*)&tmpRecv, &tmpRecvlen);
  					
								u_int8_t *mac_addr;								
			
  								if (n < 1){
    								mac_addr = NULL;
								}
								else {

									tv.tv_sec = 0;
								    tv.tv_usec = 1000;
									if (setsockopt(tmpInterface->packet_socket, SOL_SOCKET,SO_RCVTIMEO,
																				&tv,sizeof(tv)) < 0) {
										perror("Error");
									}

									struct aarp *new_aarp;
									memcpy(&new_aarp, tempBuf + sizeof(struct ether_header), 
																					sizeof(struct aarp));
									
									memcpy(&mac_addr, &new_aarp->arp_header.arp_sha, sizeof(mac_addr));

								}

			// END: send ARP request.
								
								if (mac_addr == NULL) {
					
									printf("No MAC received from ARP request");

			// START: send ICMP error - ICMP_DEST_UNREACH.

									u_int8_t tempAddr[4];
	
									memcpy(&tempAddr, tempIcmp->eth_header.ether_shost, sizeof(tempAddr));
									memcpy(tempIcmp->eth_header.ether_shost, tmpInterface->mac_addrs, 
																					   sizeof(tempAddr));
									memcpy(tempIcmp->eth_header.ether_dhost, tempAddr, sizeof(tempAddr));

									tempIcmp->eth_header.ether_type = htons(ETHERTYPE_IP);

									tempIcmp->ip_header.daddr    = tempIcmp->ip_header.saddr;
									memcpy(&tempIcmp->ip_header.saddr, &tmpInterface->ip_addrs, 
																 sizeof(tmpInterface->ip_addrs));
									tempIcmp->ip_header.check    = 0;
									tempIcmp->ip_header.frag_off = 0; 
									tempIcmp->ip_header.ihl      = 5;
									tempIcmp->ip_header.protocol = IPPROTO_ICMP; 
									tempIcmp->ip_header.tos      = 0;
									tempIcmp->ip_header.tot_len  = htons(28); 
									tempIcmp->ip_header.ttl      = 64;
									tempIcmp->ip_header.version  = 4;
								
									tempIcmp->icmp_header.type = ICMP_HOST_UNREACH;
									tempIcmp->icmp_header.checksum = 0;
									tempIcmp->icmp_header.code = ICMP_HOST_UNREACH;

									memcpy(buf, &tempIcmp, sizeof(struct iicmp));
								
									tempIcmp->icmp_header.checksum = ip_checksum(buf, sizeof(buf));
								
									memcpy(buf, &tempIcmp, sizeof(struct iicmp));

									send(tmpInterface->packet_socket, buf, sizeof(buf), 0);

			// END: send ICMP error - ICMP_DEST_UNREACH.
								}
								else {

									printf("MAC from ARP request: %02X:%02X:%02X:%02X:%02X:%02X\n", 
																	mac_addr[0], mac_addr[1], mac_addr[2], 
																	mac_addr[3], mac_addr[4], mac_addr[5]);


									tempIcmp->eth_header.ether_type = htons(ETH_P_IP);
									memcpy(tempIcmp->eth_header.ether_dhost, &mac_addr, sizeof(mac_addr));
									memcpy(tempIcmp->eth_header.ether_shost, &tmpInterface->mac_addrs, 
																sizeof(tempIcmp->eth_header.ether_shost));

									tempIcmp->icmp_header.checksum = 0;
									memcpy(buf, &tempIcmp, sizeof(struct iicmp));
									
									if (tempIcmp->ip_header.ttl == 1){

										// ICMP error - time to live

									}
									else {

										tempIcmp->ip_header.ttl = tempIcmp->ip_header.ttl - 1;
										tempIcmp->ip_header.check = 0;
										memcpy(buf, &tempIcmp, sizeof(struct iicmp));
										tempIcmp->ip_header.check = ip_checksum(buf, sizeof(buf));
										tempIcmp->icmp_header.checksum = ip_checksum(buf, sizeof(buf));

										memcpy(buf, &tempIcmp, sizeof(struct iicmp));

										send(tmpInterface->packet_socket, buf, sizeof(buf), 0);
										
									}
								}
							}
						}

// END: process ICMP echo request.

            		} 
					else {
              			printf("Forwarding Packet\n");

// START: forward packet.



// END: forward packet.
					}
				}
				else {
            		printf("Recieved packet (not ARP or ICMP), Forwarding\n");

// START: forward packet.
            		


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
