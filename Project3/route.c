/************************************************

Jarod Hanko and Norman Cunningham

CIS 457 - Project 3


References -

http://stackoverflow.com/questions/4181784/how-to-set-socket-timeout-in-c-when-making-multiple-connections

*************************************************/


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
u_int16_t icmp_checksum(void* vdata,size_t length);
int calculateIPChecksum(char *buf, int length);





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
    		printf("\n***** Got a %d byte packet *****\n", n);

			printf("PCKT - Interface Was: %s\n", prime_Interface->name);
	
			
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



			//-----------------------------------------------------------------------------------------
			// Recieved ARP.
			//-----------------------------------------------------------------------------------------
			if((ntohs(recvaddr.sll_protocol) == ETH_P_ARP)){

				printf("PCKT- New ARP packet");
			
				// The original arp packet sent to us.
				struct aarp *request_ARP;
				request_ARP = ((struct aarp*)&buf);


				// The arp packet we will send.
				struct aarp reply_ARP = *request_ARP;
				
				//---------------------------------------------------------------------------------------
				// Arp_header op_code was not arp reply
				//---------------------------------------------------------------------------------------
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
					
					//--------------------------------------------------------------------------------				 	
					// Interface ip matches arp_header ip, then we want this arp request.
					//--------------------------------------------------------------------------------
					if(ip_ARP == ip_INT){

						printf("ARP - Found interface\n");

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

						printf("ARP - Sending ARP reply\n");

						// Send the ARP reply.
						send(prime_Interface->packet_socket, &reply_ARP, sizeof(reply_ARP), 0);
					}

					//-----------------------------------------------------------------------------------
					// Interface ip doesn't match arp ip, disregard the arp request.
					//----------------------------------------------------------------------------------
					else {
					
						printf("ARP - Not our interface\n");
						printf("ARP - Disregarding request\n");

					}
				}
				//---------------------------------------------------------------------------------------
				// Arp_header op_code was arp reply
				//---------------------------------------------------------------------------------------
				else {
				
					printf("ARP - op = 'REPLY'\n");	

				}

			}

			//---------------------------------------------------------------------------------------------
			// Recieved IP.
			//---------------------------------------------------------------------------------------------
			else if ((ntohs(recvaddr.sll_protocol) == ETH_P_IP)){
				printf("PCKT - Recieved IP packet\n");


				// The original packet sent to us.
				struct iicmp *request_IICMP;
				request_IICMP = ((struct iicmp*)&buf);


				// The packet we will send.
				struct iicmp reply_IICMP = *request_IICMP;


				// Calc the ip checksum.
				int temp_checksum = request_IICMP->ip_header.check;
				reply_IICMP.ip_header.check = 0;
				char data5[1500];
				memcpy(data5, buf, 1500);
				memcpy(data5 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
				reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data5, n));


				if(temp_checksum == request_IICMP->ip_header.check){
					printf("ICMP - Good Checksum \n");
				
				

				//---------------------------------------------------------------------------------------
				// If protocol is recieve ICMP packets for all local sockets.
				//--------------------------------------------------------------------------------------
				if (request_IICMP->ip_header.protocol == IPPROTO_ICMP) {	
				

					//-----------------------------------------------------------------------------------
					// If the icmp_header type is an icmp echo or reply.
					//-----------------------------------------------------------------------------------
					if (request_IICMP->icmp_header.type == ICMP_ECHO || 
						request_IICMP->icmp_header.type == ICMP_ECHOREPLY) {

             			
						if (request_IICMP->icmp_header.type == ICMP_ECHO)							
							printf("ICMP - Received ICMP ECHO\n");
						else
							printf("ICMP - Received ICMP REPLY\n");


						//###############################################################################
						// START: process ICMP echo request.
						//################################################################################
						
						printf("ICMP - Need interface for: %X \n", request_IICMP->ip_header.daddr);
						printf("ICMP - Scanning interfaces...\n");

						// Create a temp interface pointer for looping.
						struct interface * tmp1_INT;
						tmp1_INT = interfaceList;

						// Temp char array to hold current interface name.
						char   name1A_INT[7];
						char * name1P_INT = NULL;

						//-----------------------------------------------------------------------------------
						// START: Find the correct interface given the ip by the request.
						//----------------------------------------------------------------------------------
						while (tmp1_INT != NULL){

							// Store the interface ip as a u_int32
							u_int32_t ip_INT;
							ip_INT = tmp1_INT->ip_addrs[0] | 
							 	   (tmp1_INT->ip_addrs[1] << 8) | 								
								   (tmp1_INT->ip_addrs[2] << 16) | 
								   (tmp1_INT->ip_addrs[3] << 24);

							
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
						//-----------------------------------------------------------------------------------
						// END: Find the correct interface given the ip by the request.
						//----------------------------------------------------------------------------------

	
						//------------------------------------------------------------------------------------
						// Found an interface given the ip
						//-----------------------------------------------------------------------------------
						if (name1P_INT != NULL) {
				
							printf("ICMP - Found interface: %s\n", name1P_INT);

							


							// Create a temp interface pointer for looping.
							struct interface * tmp2_INT;
							tmp2_INT = interfaceList;

							u_int8_t mac_INT[6];
					
							//----------------------------------------------------------------------------------
							// START: find mac address given the found interface
							//---------------------------------------------------------------------------------
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
							//----------------------------------------------------------------------------------
							// END: find mac address given the found interface.
							//---------------------------------------------------------------------------------


							printf("ICMP - MAC ADDRESS: %X:%X:%X:%X:%X:%X\n", mac_INT[0], mac_INT[1], mac_INT[2],
																	          mac_INT[3], mac_INT[4], mac_INT[5]);


							// Set the ether header type.
							reply_IICMP.icmp_header.type = ICMP_ECHOREPLY;


							// Clear the icmp header checksum.
							reply_IICMP.icmp_header.checksum = 0;
							

							printf("ICMP - Adjusting time to live\n");


							//---------------------------------------------------------------------------------
							// The time to live would be zero
							//---------------------------------------------------------------------------------
							if (reply_IICMP.ip_header.ttl == 1){


								printf("ICMP - This packets lifeforce has expired (ttl)\n");


								//#############################################################################			
								// START: send ICMP error - ICMP_TIME_EXCEEDED
								//#############################################################################
							
	
								// Set ether header mac address.
								memcpy(reply_IICMP.eth_header.ether_shost, prime_Interface->mac_addrs, 6);
								memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

		
								// Set ether header type.
								reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);


								// Set ip header destination and source ip address.
								reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
								memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);


								// Set other ip header fields.
								reply_IICMP.ip_header.check    = 0;
								reply_IICMP.ip_header.frag_off = 0; 
								reply_IICMP.ip_header.ihl      = 5;
								reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
								reply_IICMP.ip_header.tos      = 0;
								reply_IICMP.ip_header.tot_len  = htons(28); 
								reply_IICMP.ip_header.ttl      = 64;
								reply_IICMP.ip_header.version  = 4;
							

								// Set icmp header type and code.
								reply_IICMP.icmp_header.type = ICMP_TIME_EXCEEDED;
								reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;


								// Calc the icmp checksum.
								reply_IICMP.icmp_header.checksum = 0;
								

								// Calc the ip checksum.
								reply_IICMP.icmp_header.checksum = 0;
								reply_IICMP.ip_header.check = 0;
								char data5[1500];
								memcpy(data5, buf, 1500);
								memcpy(data5 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
								reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data5, n));


								// Calculate icmp header checksum.
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
								reply_IICMP.icmp_header.checksum = icmp_checksum(&ptr2, sizeof(ptr2));


								

								printf("ICMP - Sending ICMP error - ICMP_TIME_EXCEEDED");


								// Send on correct interface.
								send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);

								//#############################################################################			
								// END: send ICMP error - ICMP_TIME_EXCEEDED
								//#############################################################################

							}

							//---------------------------------------------------------------------------------
							// time to live was okay, update and send.
							//---------------------------------------------------------------------------------
							else {
					

								// Update time to live.
								reply_IICMP.ip_header.ttl = request_IICMP->ip_header.ttl - 1;
								reply_IICMP.ip_header.check = 0;


								// Set ip header destination and source address.
								reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
								reply_IICMP.ip_header.saddr = request_IICMP->ip_header.daddr;


								// Set the ether header destination and source mac address.
								memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);
								memcpy(reply_IICMP.eth_header.ether_shost, mac_INT, 6);



								// Calc the ip checksum.
								reply_IICMP.icmp_header.checksum = 0;
								reply_IICMP.ip_header.check = 0;
								char data5[1500];
								memcpy(data5, buf, 1500);
								memcpy(data5 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
								reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data5, n));



								// Calc icmp header checksum.
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
								reply_IICMP.icmp_header.checksum = icmp_checksum(&ptr2, sizeof(ptr2));
			

								// Combine headers with data.
								unsigned char result[sizeof(reply_IICMP) + datalength];
								memcpy(result, &reply_IICMP, sizeof(reply_IICMP));
								memcpy(result + sizeof(reply_IICMP), data, datalength);


								printf("ICMP - Sending packet\n");
								printf("ICMP - Sending on interface: %s\n", prime_Interface->name);


								// Send on correct interface.
								send(prime_Interface->packet_socket, &result, sizeof(result), 0);

							}
						}

						//------------------------------------------------------------------------------------
						// Did not find an interface given the ip
						//-----------------------------------------------------------------------------------
						else {


							printf("ICMP - Could not find an interface\n");


							// Interface to send on.
							struct interface * prize_Interface = NULL;


							// ip of the next hop.
							u_int32_t ip_HOP = 0;


							printf("ICMP - Searching routing table...\n");
							

							// Boolean to break outer loop.
							int foundMatch = 0;


							// Temp interface.
							struct interface * tmp_INT;
							tmp_INT = interfaceList;


							//-----------------------------------------------------------------------------
							// START: find next hop in routing table and correct interface to send on.
							//-----------------------------------------------------------------------------
							while (tmp_INT != NULL){


								// Temp table entry.
								struct routing_table * tmp_TBL;
								tmp_TBL = rtable;


								while (tmp_TBL != NULL){


									//---------------------------------------------------------------------
									// Found an ip in table that matched requested ip.
									//---------------------------------------------------------------------
									if (tmp_TBL->network << (32 - tmp_TBL->prefix) == 
													request_IICMP->ip_header.daddr << (32 - tmp_TBL->prefix)){
										

										//-------------------------------------------------------------------
										// Found interface that matched table interface.
										//--------------------------------------------------------------------
										if (strcmp(tmp_TBL->interface, tmp_INT->name) == 0){
										

											foundMatch = 1;
											ip_HOP = tmp_TBL->hop;
											prize_Interface = tmp_INT;
											printf("ICMP - Found interface: %s\n", prize_Interface->name);
											u_int8_t ip_print[4];
											memcpy(&ip_print, &ip_HOP, 4);
											printf("ICMP - Next hop: %X.%X.%X.%X\n", ip_print[0],ip_print[1],ip_print[2],ip_print[3]);
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

							//-----------------------------------------------------------------------------
							// END: find next hop in routing table and correct interface to send on.
							//-----------------------------------------------------------------------------
							


							//-----------------------------------------------------------------------------
							// Table look-up yielded results.
							//-----------------------------------------------------------------------------
							if (prize_Interface != NULL){

								u_int8_t * mac_HOST;

								//##########################################################################			
								// START: send ARP request.
								//#########################################################################


								// If hop was null, then hop is in packets ip header.
								if (ip_HOP == 0 || ip_HOP == -1){
								
									ip_HOP = request_IICMP->ip_header.daddr;


									// Print out the new hop address.
									u_int8_t ip_print[4];
									memcpy(&ip_print, &ip_HOP, 4);
									printf("ICMP - New hop:  %X.%X.%X.%X\n", ip_print[0],ip_print[1],ip_print[2],ip_print[3]);

								}
								
								

								// ARP packet to send.
								struct aarp *temp_ARP;
								temp_ARP = malloc(sizeof(struct aarp));
							
								// Set ether header type to arp.
								temp_ARP->eth_header.ether_type = htons(ETHERTYPE_ARP);

								// Temporary arrays.
								char broadcast_255[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast_0[6]   = {0,0,0,0,0,0};

								// Set ether header destination and source host.
								memcpy(&temp_ARP->eth_header.ether_dhost, broadcast_255, 6);
								memcpy(&temp_ARP->eth_header.ether_shost, prize_Interface->mac_addrs, 6);

								// Set arp header target and source mac address.
								memcpy(temp_ARP->arp_header.arp_tha, &broadcast_0, 6);
								memcpy(temp_ARP->arp_header.arp_sha, prize_Interface->mac_addrs, 6);
								
								// Set arp header target ip address.
								temp_ARP->arp_header.arp_tpa[3] = (uint8_t) (ip_HOP >> 24);
								temp_ARP->arp_header.arp_tpa[2] = (uint8_t) (ip_HOP >> 16);
								temp_ARP->arp_header.arp_tpa[1] = (uint8_t) (ip_HOP >> 8);
								temp_ARP->arp_header.arp_tpa[0] = (uint8_t) (ip_HOP);

								// Temporary ip address from the interface.
								u_int32_t temp_ip_INT = prize_Interface->ip_addrs[0] | 
									  		    	   (prize_Interface->ip_addrs[1] << 8) | 
						    		  		           (prize_Interface->ip_addrs[2] << 16) | 
												       (prize_Interface->ip_addrs[3] << 24);

								// Set arp header source ip address.
								temp_ARP->arp_header.arp_spa[3] = (uint8_t) (temp_ip_INT >> 24);
								temp_ARP->arp_header.arp_spa[2] = (uint8_t) (temp_ip_INT >> 16);
								temp_ARP->arp_header.arp_spa[1] = (uint8_t) (temp_ip_INT >> 8);
								temp_ARP->arp_header.arp_spa[0] = (uint8_t) (temp_ip_INT);

								// Set other arp header fields.
								temp_ARP->arp_header.ea_hdr.ar_hln = 6;
								temp_ARP->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								temp_ARP->arp_header.ea_hdr.ar_pln = 4;
								temp_ARP->arp_header.ea_hdr.ar_pro = htons(ETH_P_IP);
								temp_ARP->arp_header.ea_hdr.ar_op  = htons(ARPOP_REQUEST);

								
								printf("ICMP - Sending ARP request\n");
					
								// Send the arp request on interface that matched table look-up.
								send(prize_Interface->packet_socket, temp_ARP, sizeof(struct aarp), 0);
									
								//##########################################################################			
								// END: send ARP request.
								//#########################################################################


								//##########################################################################			
								// START: recieve ARP reply.
								//#########################################################################

								struct sockaddr_ll temp_Recv;

								// Set up time out.
  								struct timeval tv2;
  								tv2.tv_sec = 0;
  								tv2.tv_usec = 1000 * 20;

								// Add time out to interface.
  								if (setsockopt(prize_Interface->packet_socket, SOL_SOCKET, SO_RCVTIMEO,&tv2,
																					sizeof(tv2)) < 0) {
    								perror("Error");

								}

								socklen_t temp_Recvlen = sizeof(struct sockaddr_ll);

								// Char array to store packet.
								char temp_Buf[1500];

  								int n2 = recvfrom(prize_Interface->packet_socket, temp_Buf, 1500, 0, 
																(struct sockaddr*)&temp_Recv, &temp_Recvlen);
 												
								// Recieved no arp reply.
  								if (n2 < 1){
    								mac_HOST = NULL;
								}

								// Recieved arp reply.
								else {

									tv2.tv_sec = 0;
								    tv2.tv_usec = 1000;

									if (setsockopt(prize_Interface->packet_socket, SOL_SOCKET,SO_RCVTIMEO,
																				&tv2,sizeof(tv2)) < 0) {
										perror("Error");
									}

									// Create new arp structure.
									struct aarp new_aarp;

									// Copy packet int arp structure.
									memcpy(&new_aarp, temp_Buf, sizeof(struct aarp));
									
									// Allocate memory for mac address.
									mac_HOST = malloc(sizeof(u_int8_t)*6);

									// Store mac address from arp reply.
									memcpy(mac_HOST, new_aarp.arp_header.arp_sha, 6);

								}

								//##########################################################################			
								// END: recieve ARP reply.
								//##########################################################################

								
								//--------------------------------------------------------------------------
								// Did not get an arp reply. No mac address to send to.
								//---------------------------------------------------------------------------
								if (mac_HOST == NULL) {
					
									printf("ICMP - No ARP reply\n");
									printf("ICMP - Sending ICMP error - ICMP_DEST_UNREACH.\n");

									//#############################################################################	
									// START: send ICMP error - ICMP_DEST_UNREACH.
									//#############################################################################

									// Set ether header source and destination host.
									memcpy(reply_IICMP.eth_header.ether_shost, prime_Interface->mac_addrs, 6);
									memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

									// Set ether header type.
									reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);

									// Set ip header destination and source address.
									reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
									memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);

									// Set other ip header fields.
									reply_IICMP.ip_header.check    = 0;
									reply_IICMP.ip_header.frag_off = 0; 
									reply_IICMP.ip_header.ihl      = 5;
									reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
									reply_IICMP.ip_header.tos      = 0;
									reply_IICMP.ip_header.tot_len  = htons(28); 
									reply_IICMP.ip_header.ttl      = 64;
									reply_IICMP.ip_header.version  = 4;
							
									// Set icmp header type.
									reply_IICMP.icmp_header.type = ICMP_HOST_UNREACH;

									// Clear icmp header checksum.
									reply_IICMP.icmp_header.checksum = 0;

									// Set icmp error code.
									reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;



									// Calc the ip checksum.
									reply_IICMP.ip_header.check = 0;
									char data5[1500];
									memcpy(data5, buf, 1500);
									memcpy(data5 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
									reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data5, n));


									// Calculate icmp header checksum.
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
									reply_IICMP.icmp_header.checksum = icmp_checksum(&ptr2, sizeof(ptr2));


									// Send the ICMP error on original interface.
									send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);

									//#############################################################################	
									// END: send ICMP error - ICMP_DEST_UNREACH.
									//#############################################################################
								}

								//--------------------------------------------------------------------------
								// Did get an arp reply. Recieved mac address to send to.
								//---------------------------------------------------------------------------
								else {
				

									printf("ICMP - Recieved ARP reply\n");
									printf("ICMP - MAC from ARP: %02X:%02X:%02X:%02X:%02X:%02X\n", 
																	mac_HOST[0], mac_HOST[1], mac_HOST[2], 
																	mac_HOST[3], mac_HOST[4], mac_HOST[5]);


									// Set ether header type.
									reply_IICMP.eth_header.ether_type = htons(ETH_P_IP);


									// Set ether header destination and source host.
									memcpy(reply_IICMP.eth_header.ether_dhost, mac_HOST, 6);
									memcpy(reply_IICMP.eth_header.ether_shost, prize_Interface->mac_addrs, 6);


									// Clear icmp header checksum.
									reply_IICMP.icmp_header.checksum = 0;
									

									printf("ICMP - Adjusting the time to live\n");


									//---------------------------------------------------------------------------------
									// The time to live would be zero
									//---------------------------------------------------------------------------------
									if (request_IICMP->ip_header.ttl == 1){

										printf("ICMP - The packet died (ttl)\n");
										//#############################################################################	
										// START: send ICMP error - ICMP_TIME_EXCEEDED
										//#############################################################################

										// Set ether header source and destination host.
										memcpy(reply_IICMP.eth_header.ether_shost, request_IICMP->eth_header.ether_dhost, 6);
										memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

										// Set ether header type.
										reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);

										// Set ip header destination and source address.
										reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
										memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);

										// Set other ip header fields.
										reply_IICMP.ip_header.check    = 0;
										reply_IICMP.ip_header.frag_off = 0; 
										reply_IICMP.ip_header.ihl      = 5;
										reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
										reply_IICMP.ip_header.tos      = 0;
										reply_IICMP.ip_header.tot_len  = htons(28); 
										reply_IICMP.ip_header.ttl      = 64;
										reply_IICMP.ip_header.version  = 4;
							
										// Set icmp header type.
										reply_IICMP.icmp_header.type = ICMP_TIME_EXCEEDED;

										// Clear icmp header checksum.
										reply_IICMP.icmp_header.checksum = 0;

										// Set icmp error code.
										reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;



										// Calc the ip checksum.
										reply_IICMP.ip_header.check = 0;
										char data5[1500];
										memcpy(data5, buf, 1500);
										memcpy(data5 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
										reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data5, n));


										// Calculate icmp header checksum.
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
										reply_IICMP.icmp_header.checksum = icmp_checksum(&ptr2, sizeof(ptr2));


										// Send the ICMP error on original interface.
										send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);

										//#############################################################################	
										// END: send ICMP error - ICMP_TIME_EXCEEDED
										//#############################################################################
// SEND ERROR???

									}

									//---------------------------------------------------------------------------------
									// The time to live was okay.
									//---------------------------------------------------------------------------------
									else {

										// Update the time to live.
										reply_IICMP.ip_header.ttl = request_IICMP->ip_header.ttl - 1;


										// Calculate ip header checksum.
										reply_IICMP.icmp_header.checksum = 0;
										reply_IICMP.ip_header.check = 0;
										char data3[1500];
										memcpy(data3, buf, 1500);
										memcpy(data3 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
										reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data3, n));


										// Calculate icmp header checksum.
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
										reply_IICMP.icmp_header.checksum = icmp_checksum(&ptr2, sizeof(ptr2));
									

										// Combine icmp headers and data.
										unsigned char result[sizeof(reply_IICMP) + datalength];
										memcpy(result, &reply_IICMP, sizeof(reply_IICMP));
										memcpy(result + sizeof(reply_IICMP), data, datalength);

										printf("ICMP - Sending ICMP reply\n");


										// Send the packet on the correct interface.
										send(prize_Interface->packet_socket, &result, sizeof(result), 0);
	
									}
								}
							}
							//-----------------------------------------------------------------------------
							// Table look-up yielded no results.
							//-----------------------------------------------------------------------------
							else {

								printf("ICMP - Could not find an interface\n");
								printf("ICMP - Sending ICMP error - ICMP_HOST_UNREACH\n");

								//#############################################################################	
								// START: send ICMP error - ICMP_DEST_UNREACH.
								//#############################################################################

								// Set ether header source and destination mac address.
								memcpy(reply_IICMP.eth_header.ether_shost, request_IICMP->eth_header.ether_dhost, 6);
								memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);

								// Set ether header type.
								reply_IICMP.eth_header.ether_type = htons(ETHERTYPE_IP);

								// Set ip header desination and source ip address.
								reply_IICMP.ip_header.daddr = request_IICMP->ip_header.saddr;
								memcpy(&reply_IICMP.ip_header.saddr, prime_Interface->ip_addrs, 4);

								// Set other ip header fields.
								reply_IICMP.ip_header.check    = 0;
								reply_IICMP.ip_header.frag_off = 0; 
								reply_IICMP.ip_header.ihl      = 5;
								reply_IICMP.ip_header.protocol = IPPROTO_ICMP; 
								reply_IICMP.ip_header.tos      = 0;
								reply_IICMP.ip_header.tot_len  = htons(28); 
								reply_IICMP.ip_header.ttl      = 64;
								reply_IICMP.ip_header.version  = 4;

								// Set icmp header fields.						
								reply_IICMP.icmp_header.type = ICMP_HOST_UNREACH;
								reply_IICMP.icmp_header.checksum = 0;
								reply_IICMP.icmp_header.code = ICMP_HOST_UNREACH;

								// Calc the ip checksum.
								reply_IICMP.icmp_header.checksum = 0;
								reply_IICMP.ip_header.check = 0;
								char data5[1500];
								memcpy(data5, buf, 1500);
								memcpy(data5 + sizeof(struct ether_header), &reply_IICMP.ip_header, sizeof(struct iphdr));
								reply_IICMP.ip_header.check = ntohs(calculateIPChecksum(data5, n));

								// Calculate icmp header checksum.
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
								reply_IICMP.icmp_header.checksum = icmp_checksum(&ptr2, sizeof(ptr2));



								// Send on original interface.
								send(prime_Interface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);

								//#############################################################################	
								// END: send ICMP error - ICMP_DEST_UNREACH.
								//#############################################################################

							}
						}


            		} 

//#############################################################################################################
//------------------------------------------------------------------------------------------------------------
//#############################################################################################################
//-------------------------------------------------------------------------------------------------------------
//#############################################################################################################


//#######			//-----------------------------------------------------------------------------------
//FORWARD			// The icmp_header type was not an icmp echo or reply. Forward the packet.
//#######			//-----------------------------------------------------------------------------------
					else {
              			printf("PCKT - Packet Not ARP/ICMP\n");
						printf("FRWD - Packet needs to be forwarded\n");
						printf("FRWD - Scanning interfaces...\n");


						char *i_name;
						i_name = NULL;

						struct interface * iface1;

						//-----------------------------------------------------------------------------------
						// START: Find the correct interface given the ip by the request.
						//----------------------------------------------------------------------------------
						for(iface1 = interfaceList; iface1 != NULL; iface1 = iface1->next) {

							u_int32_t temp_ip;
							memcpy(&temp_ip, iface1->ip_addrs, 4);


							// The packet ip matches the interface ip.
							if(temp_ip == request_IICMP->ip_header.daddr){

								printf("FRWD - Found interface: %s\n", iface1->name);
								i_name = malloc(sizeof(7));
								memcpy(i_name, iface1->name, 7);
							  	break;
							}
						}
						//-----------------------------------------------------------------------------------
						// END: Find the correct interface given the ip by the request.
						//----------------------------------------------------------------------------------


						if(i_name != NULL) {


							u_int8_t tmp_MAC[6];

		
							//----------------------------------------------------------------------------------
							// START: Find the mac address given the interface name.
							//-----------------------------------------------------------------------------------
							struct interface * iface2;
							for(iface2 = interfaceList; iface2 != NULL; iface2 = iface2->next) {

								if(strcmp(iface2->name, i_name)){

								  	memcpy(tmp_MAC, iface2->mac_addrs, 6);
								  	break;
								}
							}
							//----------------------------------------------------------------------------------
							// START: Find the mac address given the interface name.
							//-----------------------------------------------------------------------------------
							

 							memcpy(reply_IICMP.eth_header.ether_dhost, request_IICMP->eth_header.ether_shost, 6);
							memcpy(reply_IICMP.eth_header.ether_shost, tmp_MAC, 6);
							send(prime_Interface->packet_socket, &reply_IICMP, sizeof(struct iicmp), 0);

  
						}

						else {
						  	//ICMP echo is not for us, figure out the correct interfaces to send to
						  	printf("FRWD - No interface found \n");
						  	

							// The interface to send on.
							struct interface * forwardInterface = NULL;


							// The ip to send to.
							u_int32_t forward_ip = 0;

							
							// Boolean to break outer loop.
							int breakLoop = 0;


							//-----------------------------------------------------------------------------
							// START: find next hop in routing table and correct interface to send on.
							//-----------------------------------------------------------------------------

							printf("FRWD - Scanning routing table...\n");

							// Temp interface.
							struct interface * iface2;

				
							// Loop through all interfaces.
						  	for(iface2 = interfaceList; iface2 != NULL; iface2 = iface2->next) {


								// Temp table entry.
								struct routing_table * temptable;


								// Loop through all table entries.
								for(temptable = rtable; temptable!=NULL; temptable = temptable->next) {


									// Table entry matched packet destination address.
							  		if (temptable->network << (32 - temptable->prefix) == request_IICMP->ip_header.daddr << (32 - temptable->prefix)) {
									
										if (strcmp(temptable->interface, iface2->name) == 0){
										

											forwardInterface = iface2;
											forward_ip = temptable->hop;

											printf("FRWD - Found interface: %s\n", forwardInterface->name);
											printf("FRWD - Next hop: %X\n", forward_ip);

											breakLoop = 1;
											break;
										}
							  		}
								}
								if (breakLoop){
									break;
								}
						  	}

							//-----------------------------------------------------------------------------
							// START: find next hop in routing table and correct interface to send on.
							//-----------------------------------------------------------------------------



						  	if(forwardInterface != NULL){

								u_int8_t * forward_mac = NULL;
								

								if(forward_ip == 0 || forward_ip == -1) {
								  	
									forward_ip = request_IICMP->ip_header.daddr;

									u_int8_t ip_print[4];
									memcpy(&ip_print, &forward_ip, 4);
									printf("New hop: %X.%X.%X.%X\n", ip_print[0],ip_print[1],ip_print[2],ip_print[3]);
								
								}


								//##########################################################################			
								// START: send ARP request.
								//#########################################################################


								// Struct for sending arp.
								struct aarp *temp_ARP;
								temp_ARP = malloc(sizeof(struct aarp));
						

								// Set ether header type.
								temp_ARP->eth_header.ether_type = htons(ETHERTYPE_ARP);


								// Temp arrays.
								char broadcast_255[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast_0[6]   = {0,0,0,0,0,0};


								// Set ether header destination and source mac address.
								memcpy(&temp_ARP->eth_header.ether_dhost, broadcast_255, 6);
								memcpy(&temp_ARP->eth_header.ether_shost, forwardInterface->mac_addrs, 6);


								// Set arp header target and source mac addess.
								memcpy(temp_ARP->arp_header.arp_tha, &broadcast_0, 6);
								memcpy(temp_ARP->arp_header.arp_sha, forwardInterface->mac_addrs, 6);
							

								// Set arp header target ip address.
								temp_ARP->arp_header.arp_tpa[3] = (uint8_t) (forward_ip >> 24);
								temp_ARP->arp_header.arp_tpa[2] = (uint8_t) (forward_ip >> 16);
								temp_ARP->arp_header.arp_tpa[1] = (uint8_t) (forward_ip >> 8);
								temp_ARP->arp_header.arp_tpa[0] = (uint8_t) (forward_ip);


								// Temp interface ip address.
								u_int32_t temp_ip_INT = forwardInterface->ip_addrs[0] | 
									  		    	   (forwardInterface->ip_addrs[1] << 8) | 
									  		           (forwardInterface->ip_addrs[2] << 16) | 
													   (forwardInterface->ip_addrs[3] << 24);


								// Set arp header source ip address.
								temp_ARP->arp_header.arp_spa[3] = (uint8_t) (temp_ip_INT >> 24);
								temp_ARP->arp_header.arp_spa[2] = (uint8_t) (temp_ip_INT >> 16);
								temp_ARP->arp_header.arp_spa[1] = (uint8_t) (temp_ip_INT >> 8);
								temp_ARP->arp_header.arp_spa[0] = (uint8_t) (temp_ip_INT);


								// Set other arp header fields.
								temp_ARP->arp_header.ea_hdr.ar_hln = 6;
								temp_ARP->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								temp_ARP->arp_header.ea_hdr.ar_pln = 4;
								temp_ARP->arp_header.ea_hdr.ar_pro = htons(ETH_P_IP);
								temp_ARP->arp_header.ea_hdr.ar_op  = htons(ARPOP_REQUEST);

							
								printf("FRWD - Sending ARP request\n");
				

								// Send on correct interface.
								send(forwardInterface->packet_socket, temp_ARP, sizeof(struct aarp), 0);
								
							
								//##########################################################################			
								// END: send ARP request.
								//#########################################################################


								//##########################################################################			
								// START: recieve ARP reply.
								//#########################################################################


								struct sockaddr_ll temp_Recv;

								struct timeval tv2;
								tv2.tv_sec = 0;
								tv2.tv_usec = 1000 * 20;

								if (setsockopt(forwardInterface->packet_socket, SOL_SOCKET, SO_RCVTIMEO,&tv2,
																					sizeof(tv2)) < 0) {
									perror("Error");
								}

								socklen_t temp_Recvlen = sizeof(struct sockaddr_ll);
								char temp_Buf[1500];

								int n2 = recvfrom(forwardInterface->packet_socket, temp_Buf, 1500, 0, 
																(struct sockaddr*)&temp_Recv, &temp_Recvlen);
											
		
								if (n2 < 1){
									forward_mac = NULL;
								}
								else {

									tv2.tv_sec = 0;
									tv2.tv_usec = 1000;

									if (setsockopt(forwardInterface->packet_socket, SOL_SOCKET,SO_RCVTIMEO,
																				&tv2,sizeof(tv2)) < 0) {
										perror("Error");
									}


									// Create temp arp to hold reply.
									struct aarp new_aarp;


									// Copy packet conteents into temp arp.
									memcpy(&new_aarp, temp_Buf, sizeof(struct aarp));
								

									// Allocate memory for mac address pointer.
									forward_mac = malloc(sizeof(u_int8_t)*6);


									// Copy in mac address from arp reply .
									memcpy(forward_mac, new_aarp.arp_header.arp_sha, 6);

								}

								//##########################################################################			
								// END: recieve ARP reply.
								//#########################################################################


								if(forward_mac == NULL){


								  	printf("FRWD - No ARP reply\n");
// SEND ERROR???
									


								}
								else{

									// Print the recieved mac address. 
								  	printf("FRWD - ARP returned MAC: ");
								  	printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
									forward_mac[0],forward_mac[1],
									forward_mac[2],forward_mac[3],
									forward_mac[4],forward_mac[5]);

								  	
									// Set ether header destination and source mac address.
									memcpy(reply_IICMP.eth_header.ether_dhost, forward_mac, 6);
									memcpy(reply_IICMP.eth_header.ether_shost, forwardInterface->mac_addrs, 6);


									// Set ether header type.
									reply_IICMP.eth_header.ether_type = htons(ETH_P_IP);


									char result[sizeof(buf)];
									memcpy(result, &buf, sizeof(buf));
									memcpy(result, &reply_IICMP, sizeof(reply_IICMP));


									printf("FRWD - Forwarding packet.\n");

									// Send packet on the correct interface.
							  		send(forwardInterface->packet_socket, &result, sizeof(result), 0);

									// Send on correct interface.
								  	//send(forwardInterface->packet_socket, &reply_IICMP, sizeof(reply_IICMP), 0);
								}

						  	}
						  	else{
// SEND ERROR???
								printf("FRWD - Address not found in table\n");


						  	}
						}
					
					}
					}

	//#######		//-------------------------------------------------------------------------------------
	//FORWARD		// Protocol is not, recieve ICMP packets for all local sockets. Forward the packet.
	//#######		//-------------------------------------------------------------------------------------
					else {

		        		printf("PCKT - Packet Not ARP/ICMP\n");
						printf("FRWD - Packet needs to be forwarded\n");
						printf("FRWD - Scanning interfaces...\n");


						// Original packet contents.
						struct iip *request_IIP;
						request_IIP = ((struct iip*)&buf);


						// The packet we will send.
						struct iip reply_IIP = *request_IIP;
		        		

						// Interface name.
						char *i_name = NULL;
			

						// Temp interface for loop.
						struct interface * iface1;

						//-----------------------------------------------------------------------------------
						// START: Find the correct interface given the ip by the request.
						//----------------------------------------------------------------------------------
						for(iface1 = interfaceList; iface1 != NULL; iface1 = iface1->next) {


							// Interface ip.
							u_int32_t temp_ip;
							memcpy(&temp_ip, iface1->ip_addrs, 4);


							// If interface ip matches the ip of the packet.
							if(temp_ip == request_IIP->ip_header.daddr){

								printf("FRWD - Found interface: %s\n", iface1->name);
								memcpy(i_name, iface1->name, 7);
							  	break;
							}
						}
						//-----------------------------------------------------------------------------------
						// END: Find the correct interface given the ip by the request.
						//----------------------------------------------------------------------------------


						if(i_name != NULL) {


							u_int8_t tmp_MAC[6];


							//----------------------------------------------------------------------------------
							// START: Find the mac address given the interface name.
							//-----------------------------------------------------------------------------------
							struct interface * iface2;
							for(iface2 = interfaceList; iface2 != NULL; iface2 = iface2->next) {


								// If the name matched the interface name.
								if(strcmp(iface2->name, i_name)){


								  	memcpy(tmp_MAC, iface2->mac_addrs, 6);
								  	break;
								}
							}
							//----------------------------------------------------------------------------------
							// END: Find the mac address given the interface name.
							//-----------------------------------------------------------------------------------
						

							// Set ether header destination and source mac address.
							memcpy(reply_IIP.eth_header.ether_dhost, request_IIP->eth_header.ether_shost, 6);
							memcpy(reply_IIP.eth_header.ether_shost, tmp_MAC, 6);
						
							// Send on original interface.
							send(prime_Interface->packet_socket, &reply_IICMP, sizeof(struct iicmp), 0);


						}

						else {


						  	printf("FRWD - No interface found\n");
							printf("FRWD - Scanning routing table...\n");
						  	
							// The interface to send on.
							struct interface * forwardInterface = NULL;
							u_int32_t forward_ip = 0;

							// Boolean to break outer loop.
							int breakLoop = 0;

							//-----------------------------------------------------------------------------
							// START: find next hop in routing table and correct interface to send on.
							//-----------------------------------------------------------------------------

							// Temp interface.
							struct interface * iface2;
						
							// Loop through all the interfaces.
						  	for(iface2 = interfaceList; iface2 != NULL; iface2 = iface2->next) {

								// Temp table entry.
								struct routing_table * temptable;

								// Loop through all the table entries.
								for(temptable = rtable; temptable!=NULL; temptable = temptable->next) {

									// If the table ip matches the packet ip.
							  		if (temptable->network << (32 - temptable->prefix) == request_IIP->ip_header.daddr << (32 - temptable->prefix)) {
								
										// If the interface name matches the table entry name.
										if (strcmp(temptable->interface, iface2->name) == 0){

											forwardInterface = iface2;
											forward_ip = temptable->hop;

											printf("FRWD - Found interface: %s\n", forwardInterface->name);
											printf("FRWD - Next hop: %X\n", forward_ip);

											breakLoop = 1;
											break;
										}
							  		}
								}
								if (breakLoop){
									break;
								}
						  	}
							//-----------------------------------------------------------------------------
							// END: find next hop in routing table and correct interface to send on.
							//-----------------------------------------------------------------------------


							//-----------------------------------------------------------------------------
							// Found a match in routing table.
							//-----------------------------------------------------------------------------
						  	if(forwardInterface != NULL){


								// The mac address we are forwarding to.
								u_int8_t * forward_mac;
							
							
								// If the hop in the routing table was null, then use packets ip.
								if(forward_ip == 0 || forward_ip == -1) {
								  	
									forward_ip = request_IIP->ip_header.daddr;
							
									u_int8_t ip_print[4];
									memcpy(&ip_print, &forward_ip, 4);
									printf("FRWD - New hop: %X.%X.%X.%X\n", ip_print[0],ip_print[1],ip_print[2],ip_print[3]);

							
								}
			

								//##########################################################################			
								// START: send ARP request.
								//#########################################################################


								// Struct for sending arp.
								struct aarp *temp_ARP;
								temp_ARP = malloc(sizeof(struct aarp));
						

								// Set ether header type.
								temp_ARP->eth_header.ether_type = htons(ETHERTYPE_ARP);


								// Temp arrays.
								char broadcast_255[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
								char broadcast_0[6]   = {0,0,0,0,0,0};


								// Set ether header destination and source mac address.
								memcpy(&temp_ARP->eth_header.ether_dhost, broadcast_255, 6);
								memcpy(&temp_ARP->eth_header.ether_shost, forwardInterface->mac_addrs, 6);


								// Set arp header target and source mac addess.
								memcpy(temp_ARP->arp_header.arp_tha, &broadcast_0, 6);
								memcpy(temp_ARP->arp_header.arp_sha, forwardInterface->mac_addrs, 6);
							

								// Set arp header target ip address.
								temp_ARP->arp_header.arp_tpa[3] = (uint8_t) (forward_ip >> 24);
								temp_ARP->arp_header.arp_tpa[2] = (uint8_t) (forward_ip >> 16);
								temp_ARP->arp_header.arp_tpa[1] = (uint8_t) (forward_ip >> 8);
								temp_ARP->arp_header.arp_tpa[0] = (uint8_t) (forward_ip);


								// Temp interface ip address.
								u_int32_t temp_ip_INT = forwardInterface->ip_addrs[0] | 
									  		    	   (forwardInterface->ip_addrs[1] << 8) | 
									  		           (forwardInterface->ip_addrs[2] << 16) | 
													   (forwardInterface->ip_addrs[3] << 24);


								// Set arp header source ip address.
								temp_ARP->arp_header.arp_spa[3] = (uint8_t) (temp_ip_INT >> 24);
								temp_ARP->arp_header.arp_spa[2] = (uint8_t) (temp_ip_INT >> 16);
								temp_ARP->arp_header.arp_spa[1] = (uint8_t) (temp_ip_INT >> 8);
								temp_ARP->arp_header.arp_spa[0] = (uint8_t) (temp_ip_INT);


								// Set other arp header fields.
								temp_ARP->arp_header.ea_hdr.ar_hln = 6;
								temp_ARP->arp_header.ea_hdr.ar_hrd = htons(ARPHRD_ETHER);
								temp_ARP->arp_header.ea_hdr.ar_pln = 4;
								temp_ARP->arp_header.ea_hdr.ar_pro = htons(ETH_P_IP);
								temp_ARP->arp_header.ea_hdr.ar_op  = htons(ARPOP_REQUEST);

							
								printf("FRWD - Sending ARP request\n");
				

								// Send on correct interface.
								send(forwardInterface->packet_socket, temp_ARP, sizeof(struct aarp), 0);
								
							
								//##########################################################################			
								// END: send ARP request.
								//#########################################################################


								//##########################################################################			
								// START: recieve ARP reply.
								//#########################################################################


								struct sockaddr_ll temp_Recv;

								struct timeval tv2;
								tv2.tv_sec = 0;
								tv2.tv_usec = 1000 * 20;

								if (setsockopt(forwardInterface->packet_socket, SOL_SOCKET, SO_RCVTIMEO,&tv2,
																					sizeof(tv2)) < 0) {
									perror("Error");
								}

								socklen_t temp_Recvlen = sizeof(struct sockaddr_ll);
								char temp_Buf[1500];

								int n2 = recvfrom(forwardInterface->packet_socket, temp_Buf, 1500, 0, 
																(struct sockaddr*)&temp_Recv, &temp_Recvlen);
											
		
								if (n2 < 1){
									forward_mac = NULL;
								}
								else {

									tv2.tv_sec = 0;
									tv2.tv_usec = 1000;

									if (setsockopt(forwardInterface->packet_socket, SOL_SOCKET,SO_RCVTIMEO,
																				&tv2,sizeof(tv2)) < 0) {
										perror("Error");
									}


									// Create temp arp to hold reply.
									struct aarp new_aarp;


									// Copy packet conteents into temp arp.
									memcpy(&new_aarp, temp_Buf, sizeof(struct aarp));
								

									// Allocate memory for mac address pointer.
									forward_mac = malloc(sizeof(u_int8_t)*6);


									// Copy in mac address from arp reply .
									memcpy(forward_mac, new_aarp.arp_header.arp_sha, 6);

								}

								//##########################################################################			
								// END: recieve ARP reply.
								//#########################################################################



								if(forward_mac == NULL){
// SEND ERRROR???
								  	//send ICMP error
								  	printf("FRWD - No ARP reply\n");
									


								}
								else{

									// Print mac address from arp reply.
								  	printf("FRWD - ARP returned MAC: ");
								  	printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
									forward_mac[0],forward_mac[1],
									forward_mac[2],forward_mac[3],
									forward_mac[4],forward_mac[5]);

								  	
									// Set ether header destination and source mac address.
									memcpy(reply_IIP.eth_header.ether_dhost, forward_mac, 6);
									memcpy(reply_IIP.eth_header.ether_shost, forwardInterface->mac_addrs, 6);

									// Set ether header type.
									reply_IIP.eth_header.ether_type = htons(ETH_P_IP);


									char result[sizeof(buf)];
									memcpy(result, &buf, sizeof(buf));
									memcpy(&result, &reply_IIP, sizeof(reply_IIP));


									printf("FRWD - Sending packet");

									// Send packet on the correct interface.
								  	send(forwardInterface->packet_socket, &result, sizeof(result), 0);
								}

						  	}

							//-----------------------------------------------------------------------------
							// No  match in routing table.
							//-----------------------------------------------------------------------------
						  	else{
// SEND ERROR???

								printf("FRWD - Could not find an interface\n");
								
						  	}
						}
					}
				
				}
				else {
					printf("--- Bad Checksum ---\n");
					printf("PCKT - The packet was dropped right into the trash.\n");
//SEND ERROR??
				}
			}
		}
	}
	return 0;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++





/****************************************************************************************
* LOAD ROUTING TABLE 	
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


/****************************************************************************************
* Checksums
****************************************************************************************/
//icmp checksum calculator from
//source: http://www.microhowto.info/howto/calculate_an_internet_protocol_checksum_in_c.html
u_int16_t icmp_checksum(void* vdata,size_t length) {
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



int calculateIPChecksum(char* buf, int length)
{
  int lengthToIP = sizeof(struct ether_header);
  int ipLength = sizeof(struct iphdr);

  struct iphdr ipHeader;
  memcpy(&ipHeader, &buf + lengthToIP, sizeof(struct iphdr));

  unsigned int checksum = 0;

  int i;
  for(i = 0; i < ipLength; i+=2)
  {
    checksum += (uint32_t) ((uint8_t) buf[lengthToIP+i] << 8 | (uint8_t) buf[lengthToIP + i + 1]);
  }

  checksum = (checksum >> 16) + (checksum & 0xffff);
  return (uint16_t) ~checksum;
}


