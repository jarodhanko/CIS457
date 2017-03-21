#include <sys/socket.h> 
#include <netpacket/packet.h> 
#include <net/ethernet.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/if_ether.h>
#include <string.h>

struct aarp {
	struct ether_header eth_header;
	struct ether_arp arp_header;
};


int main(){
  int packet_socket;
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
    if(tmp->ifa_addr->sa_family==AF_PACKET){
      printf("Interface: %s\n",tmp->ifa_name);
      //create a packet socket on interface r?-eth1
      if(!strncmp(&(tmp->ifa_name[3]),"eth1",4)){
	printf("Creating Socket on interface %s\n",tmp->ifa_name);
	//create a packet socket
	//AF_PACKET makes it a packet socket
	//SOCK_RAW makes it so we get the entire packet
	//could also use SOCK_DGRAM to cut off link layer header
	//ETH_P_ALL indicates we want all (upper layer) protocols
	//we could specify just a specific one
	packet_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
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
      }
    }
  }
  //free the interface list when we don't need it anymore
  freeifaddrs(ifaddr);

  //loop and recieve packets. We are only looking at one interface,
  //for the project you will probably want to look at more (to do so,
  //a good way is to have one socket per interface and use select to
  //see which ones have data)
  printf("Ready to recieve now\n");
  while(1){
    char buf[1500];
    struct sockaddr_ll recvaddr;
    int recvaddrlen=sizeof(struct sockaddr_ll);
    //we can use recv, since the addresses are in the packet, but we
    //use recvfrom because it gives us an easy way to determine if
    //this packet is incoming or outgoing (when using ETH_P_ALL, we
    //see packets in both directions. Only outgoing can be seen when
    //using a packet socket with some specific protocol)
    int n = recvfrom(packet_socket, buf, 1500,0,(struct sockaddr*)&recvaddr, &recvaddrlen);
    //ignore outgoing packets (we can't disable some from being sent
    //by the OS automatically, for example ICMP port unreachable
    //messages, so we will just ignore them here)
    if(recvaddr.sll_pkttype==PACKET_OUTGOING)
      continue;
    //start processing all others
    printf("Got a %d byte packet\n", n);
	
	int i;
	for (i = 0; i < n; i++)
	{
		if (i > 0) printf(":");
		printf("%02X", buf[i]);
	}
	printf("\n");
    
	struct aarp *request;
	request = ((struct aarp*)&buf);

	printf("ETHER DEST: %02X%02X%02X%02X%02X%02X \n", request->eth_header.ether_dhost[0], request->eth_header.ether_dhost[1], request->eth_header.ether_dhost[2], request->eth_header.ether_dhost[3], request->eth_header.ether_dhost[4], request->eth_header.ether_dhost[5]);
	printf("ETHER SRC: %02X%02X%02X%02X%02X%02X \n", request->eth_header.ether_shost[0], request->eth_header.ether_shost[1], request->eth_header.ether_shost[2], request->eth_header.ether_shost[3], request->eth_header.ether_shost[4], request->eth_header.ether_shost[5]);
	printf("ETHER TYPE: %02X \n", ntohs(request->eth_header.ether_type));
	printf("ARP FORMAT HARD ADDR: %02X \n", ntohs(request->arp_header.ea_hdr.ar_hrd));
	printf("ARP FORMAT PROTO ADDR: %02X \n", ntohs(request->arp_header.ea_hdr.ar_pro));
	printf("ARP LEN HARD ADDR: %02X \n", request->arp_header.ea_hdr.ar_hln);
	printf("ARP LEN PROTO ADDR: %02X \n", request->arp_header.ea_hdr.ar_pln);
	printf("ARP OP: %02X \n", ntohs(request->arp_header.ea_hdr.ar_op));
	printf("ARP SENDER HARD ADDR: %02X%02X%02X%02X%02X%02X \n", request->arp_header.arp_sha[0], request->arp_header.arp_sha[1], request->arp_header.arp_sha[2], request->arp_header.arp_sha[3], request->arp_header.arp_sha[4], request->arp_header.arp_sha[5]);
	printf("ARP SENDER PROTO ADDR: %02X%02X%02X%02X \n", request->arp_header.arp_spa[0], request->arp_header.arp_spa[1], request->arp_header.arp_spa[2], request->arp_header.arp_spa[3]);
	printf("ARP TARGET HARD ADDR: %02X%02X%02X%02X%02X%02X \n", request->arp_header.arp_tha[0], request->arp_header.arp_tha[1], request->arp_header.arp_tha[2], request->arp_header.arp_tha[3]);
	printf("ARP TARGET PROTO ADDR: %02X%02X%02X%02X \n", request->arp_header.arp_tpa[0], request->arp_header.arp_tpa[1], request->arp_header.arp_tpa[2], request->arp_header.arp_tpa[3]);

	struct aarp reply = *request;
	u_int8_t tmp[6] = {0xa2, 0x22, 0xdd, 0xfc, 0x5c, 0x89};
	memcpy(reply.eth_header.ether_shost, tmp, ETH_ALEN);
	memcpy(reply.eth_header.ether_dhost, request->eth_header.ether_shost, ETH_ALEN);
	
	//ether_type is same
	//arp format hard addr is the same
	//arp format proto addr is the same
	//arp len hard addr is the same
	//arp len proto addr is the same
	reply.arp_header.ea_hdr.ar_op=htons(ARPOP_REPLY);
	memcpy(reply.arp_header.arp_sha, tmp, 6);

	u_int8_t tmp2[4] = {10, 1, 0, 1};
	memcpy(reply.arp_header.arp_spa, tmp2, 4);

	memcpy(reply.arp_header.arp_tha, request->arp_header.arp_sha, ETH_ALEN);
	memcpy(reply.arp_header.arp_tpa, request->arp_header.arp_spa, 4);

	send(packet_socket, &reply, sizeof(reply), 0);
    //what else to do is up to you, you can send packets with send,
    //just like we used for TCP sockets (or you can use sendto, but it
    //is not necessary, since the headers, including all addresses,
    //need to be in the buffer you are sending)

	//check if packet is an arp request

	//if so, construct and send an arp response
	//see struct ether_arp in if_ether.h
	//sendArpResponse(n)

	//check if packet is an icmp request
	
	//if so, construct and send an icmp response
	//sendICMPResponse(n)    

  }
  //exit
  return 0;
}
