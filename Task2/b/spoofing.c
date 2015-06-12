#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>

/*
Internet checksum function (from BSD Tahoe) 
We can use this function to calculate checksums for all layers. 
ICMP protocol mandates checksum, so we have to calculate it.
*/
unsigned short in_cksum(unsigned short *addr, int len)
{
  int nleft = len;
  int sum = 0;
  unsigned short *w = addr;
  unsigned short answer = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= 2;
  }

  if (nleft == 1) {
    *(unsigned char *) (&answer) = *(unsigned char *) w;
    sum += answer;
  }
  
  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  answer = ~sum;
  return (answer);
}

int main(int argc, char **argv)
{
  struct ip ip;
  struct udphdr udp;
  struct icmp icmp;
  int sd;
  const int on = 1;
  struct sockaddr_in sin;
  u_char* packet;

  // Grab some space for our packet:
  packet = (u_char *)malloc(60);
  
  //IP Layer header construct

  /* Fill Layer II (IP protocol) fields... 
     Header length (including options) in units of 32 bits (4 bytes).
     Assuming we will not send any IP options, 
     IP header length is 20 bytes, 
     so we need to stuff (20 / 4 = 5 here):
  */
  ip.ip_hl = 0x5;
  //Protocol Version is 4, meaning Ipv4:
  ip.ip_v = 0x4;
  //Type of Service. Packet precedence:
  ip.ip_tos = 0x0;
  /*Total length for our packet require to be converted to the network
    byte-order(htons(60), but MAC OS doesn't need this):*/
  ip.ip_len = 60;
  //ID field uniquely identifies each datagram sent by this host:
  ip.ip_id = 0;
  /*Fragment offset for our packet. 
    We set this to 0x0 since we don't desire any fragmentation:*/
  ip.ip_off = 0x0;
  /*Time to live. 
    Maximum number of hops that the packet can 
    pass while travelling through its destination.*/
  ip.ip_ttl = 64;
  //Upper layer (Layer III) protocol number:
  ip.ip_p = IPPROTO_ICMP;
  /*We set the checksum value to zero before passing the packet
    into the checksum function. Note that this checksum is 
    calculate over the IP header only. Upper layer protocols 
    have their own checksum fields, and must be calculated seperately.*/
  ip.ip_sum = 0x0;
  /*Source IP address, this might well be any IP address that 
    may or may NOT be one of the assigned address to one of our interfaces:*/
  ip.ip_src.s_addr = inet_addr("192.168.142.238");
  //  Destination IP address:
  ip.ip_dst.s_addr = inet_addr("172.16.87.254");
  /*We pass the IP header and its length into the internet checksum
    function. The function returns us as 16-bit checksum value for
    the header:*/
  ip.ip_sum = in_cksum((unsigned short *)&ip, sizeof(ip));
  //printf("ip cksum: %x\n", ip.ip_sum);
  /*We're finished preparing our IP header. Let's copy it into 
    the very begining of our packet:*/
  memcpy(packet, &ip, sizeof(ip));
  
  
  //ICMP header construct
  
  //As for Layer III (ICMP) data, Icmp type 8 for echo request:
  icmp.icmp_type = ICMP_ECHO;
  //  Code 0. Echo Request.
  icmp.icmp_code = 0;
  //ID. random number:
  icmp.icmp_id = htons(50179);
  //Icmp sequence number use htons to transform big endian:
  icmp.icmp_seq = htons(0x0);
  /*Just like with the Ip header, we set the ICMP header 
    checksum to zero and pass the icmp packet into the 
    cheksum function. We store the returned value in the 
    checksum field of ICMP header:*/
  icmp.icmp_cksum = 0;
  printf("chksum: %x\n",in_cksum((unsigned short *)&icmp, sizeof(icmp)));
  icmp.icmp_cksum = htons(0x8336);//in_cksum((unsigned short *)&icmp, 8);
  //We append the ICMP header to the packet at offset 20:
  memcpy(packet + 20, &icmp, 8);
  /*We crafted our packet byte-by-byte. It's time we inject
    it into the network. First create our raw socket:*/
  if ((sd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
    perror("raw socket");
    exit(1);
  }
  /*We tell kernel that we've also prepared the IP header; 
    there's nothing that the IP stack will do about it:*/
  if (setsockopt(sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
    perror("setsockopt");
    exit(1);
  }
  /*Still, the kernel is going to prepare Layer I data for us. 
    For that, we need to specify a destination for the kernel in
    order for it to decide where to send the raw datagram. We fill
    in a struct in_addr with the desired destination IP address, 
    and pass this structure to the sendto(2) or sendmsg(2) system calls:*/
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = ip.ip_dst.s_addr;
  /*As for writing the packet... We cannot use send(2) 
    system call for this, since the socket is not a "connected"
    type of socket. As stated in the above paragraph, we need 
    to tell where to send the raw IP datagram. sendto(2) and 
    sendmsg(2) system calls are designed to handle this:*/
  if (sendto(sd, packet, 60, 0, (struct sockaddr *)&sin, 
	     sizeof(struct sockaddr)) < 0)  {
    perror("sendto");
    exit(1);
  }
  
  return 0;
}
