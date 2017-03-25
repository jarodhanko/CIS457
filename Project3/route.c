#include <sys/socket.h> 
#include <netpacket/packet.h> 
#include <net/ethernet.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/if_ether.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>


struct aarp {
	struct ether_header eth_header;
	struct ether_arp arp_header;
} __attribute__ ((__packed__));

struct iicmp{
	struct ether_header eth_header;
	struct iphdr ip_header;
	struct icmphdr icmp_header;
	char *data;
} __attribute__ ((__packed__));

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


unsigned short ip2_checksum(void *b, int len)
{	unsigned short *buf = b;
	unsigned int sum=0;
	unsigned short result;

	for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	if ( len == 1 )
		sum += *(unsigned char*)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	return result;
}


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

	char buf2[1500];
	memcpy(buf2, buf, sizeof(buf));
	request = ((struct aarp*)&buf);

	if(ntohs(request->eth_header.ether_type) == ETHERTYPE_ARP){

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
		u_int8_t tmp[6] = {0x52, 0x46, 0x9d, 0x78, 0xeb, 0xfd};
		memcpy(reply.eth_header.ether_shost, tmp, ETH_ALEN);
		memcpy(reply.eth_header.ether_dhost, request->eth_header.ether_shost, ETH_ALEN);

		reply.arp_header.ea_hdr.ar_op=htons(ARPOP_REPLY);
		memcpy(reply.arp_header.arp_sha, tmp, 6);

		u_int8_t tmp2[4] = {10, 1, 0, 1};
		memcpy(reply.arp_header.arp_spa, tmp2, 4);

		memcpy(reply.arp_header.arp_tha, request->arp_header.arp_sha, ETH_ALEN);
		memcpy(reply.arp_header.arp_tpa, request->arp_header.arp_spa, 4);

		send(packet_socket, &reply, sizeof(reply), 0);
	}else if(ntohs(request->eth_header.ether_type) == ETHERTYPE_IP){
		struct iicmp request2;
		request2 = *((struct iicmp*)&buf2);
		int datalength = ntohs(request2.ip_header.tot_len) - sizeof(request2.ip_header) - sizeof(request2.icmp_header);
		printf("\n THE DATA LENGTH IS %d", datalength);
		if(datalength > 0)
			memcpy(request2.data, buf2 + sizeof(request2), datalength);
		unsigned char tmp3[] = {buf2[26], buf2[27], buf2[28], buf2[29]};
		unsigned char tmp4[] = {buf2[30], buf2[31], buf2[32], buf2[33]};
		
		struct iicmp reply = request2;
		printf("\n \t \t THE SIZE IS: %d \n", sizeof(request2));
		memcpy(&reply, &request2, 98);

		u_int8_t tmp[6] = {0xa2, 0x22, 0xdd, 0xfc, 0x5c, 0x89};
		memcpy(reply.eth_header.ether_shost, tmp, ETH_ALEN);
		memcpy(reply.eth_header.ether_dhost, request2.eth_header.ether_shost, ETH_ALEN);


		memcpy(&reply.ip_header.daddr, tmp3, 4);
		memcpy(&reply.ip_header.saddr, tmp4, 4);

		reply.icmp_header.type = ICMP_ECHOREPLY;
		reply.icmp_header.checksum = 0;

		printf("\n \t SIZEOFHEADER: %02X \n", sizeof(reply.icmp_header));
		reply.icmp_header.checksum = ip_checksum(&reply.icmp_header, sizeof(reply.icmp_header));


		printf("\n IPHDR_len: %02X \n SIZEOF: %d \n", reply.ip_header.ihl, sizeof(reply));

		printf("ETHER DEST: %02X%02X%02X%02X%02X%02X \n", reply.eth_header.ether_dhost[0], reply.eth_header.ether_dhost[1], reply.eth_header.ether_dhost[2], reply.eth_header.ether_dhost[3], reply.eth_header.ether_dhost[4], reply.eth_header.ether_dhost[5]);
		printf("ETHER SRC: %02X%02X%02X%02X%02X%02X \n", reply.eth_header.ether_shost[0], reply.eth_header.ether_shost[1], reply.eth_header.ether_shost[2], reply.eth_header.ether_shost[3], reply.eth_header.ether_shost[4], reply.eth_header.ether_shost[5]);
		printf("ETHER TYPE: %02X \n", ntohs(reply.eth_header.ether_type));
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
		printf("IP DADDR: %02X \n", ntohl(reply.ip_header.daddr));
		printf("ICMP TYPE: %02X \n", reply.icmp_header.type);
		printf("ICMP CODE: %02X \n", reply.icmp_header.code);
		printf("ICMP CHECKSUM: %02X \n", ntohs(reply.icmp_header.checksum));

		
		send(packet_socket, &reply, sizeof(reply), 0);
	}

  }
  //exit
  return 0;
}
