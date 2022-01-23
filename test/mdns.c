#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include "../utils.h"
#include "../netUtils.h"

#include "../dns/dns.h"
#include "../dns/mappings.h"
#include "../dns/output.h"


void createQuery(dns_packet_t* packet, size_t* size){
    static int count=0;
  dns_question_t domain;
  dns_query_t    query;
  edns0_opt_t    opt;
  dns_answer_t   edns[2];
  domain.name  = "bridge.hn.local.";
  domain.type  = dns_type_value("TXT");
  domain.class = CLASS_IN;

  query.id          = 1234;     /* should be a random value */
  query.query       = true;
  query.opcode      = OP_QUERY;
  query.aa          = false;
  query.tc          = false;
  query.rd          = true;
  query.ra          = false;
  query.z           = false;
  query.ad          = false;
  query.cd          = false;
  query.rcode       = RCODE_OKAY;
  query.qdcount     = 1;
  query.questions   = &domain;
  query.ancount     = 0;
  query.answers     = NULL;
  query.nscount     = 0;
  query.nameservers = NULL;
  query.arcount     = 0;
  query.additional  = NULL;

  ///////////////////////////

    query.ancount     = 2;
    
    struct sockaddr_in ip;
    str_toIpAddr((struct sockaddr*)&ip,"192.169.29.90");

    // Setup TXT record answer
    char txt[100]="";
    sprintf(txt,"PORT=2000;meow=count-%d",count);
    count++;
    edns[0].txt.name      = "bridge.hn.local.";
    edns[0].txt.text        = txt;
    edns[0].txt.type        = RR_TXT;
    edns[0].txt.class       = CLASS_IN;
    edns[0].txt.ttl         = 0;
    edns[0].txt.len = str_len(txt);

    // Setup A record answer
    edns[1].a.name      = "bridge.hn.local.";
    edns[1].a.class       = CLASS_IN;
    edns[1].a.type       = RR_A;
    edns[1].a.ttl         = 0;
    edns[1].a.address= ntohl(ip.sin_addr.s_addr);
    
    query.answers = edns;

dns_rcode_t rc = dns_encode(packet,size,&query);
printf("size of encoded packet: %d\n", (int) *size);
}

void sendQuery(){
dns_packet_t   request[DNS_BUFFER_UDP];
        size_t         reqsize;
        reqsize = sizeof(request);
        createQuery(request,&reqsize);
        int r=mdns_send((void*)request,(int)reqsize);
        if(r<=0){
            printf("Failed to send packet\n");
        }
        else{
            printf("Sent packet of size %d\n",r);
        }
}

int main(int argc, char *argv[])
{
    struct sockaddr ip;
    Socket sock;
    if(!mdns_start(&sock))
    {
        printf("Failed to start mDNS\n");
        return 1;
    }
    sock_setNonBlocking(&sock);
    printf("mDNS started.\n");
    int n=0;
    while(1){
        char buffer[256];
        n=udp_read(buffer,256,&sock,&ip);
        if(n > 0){
            char ipstr[100];
            ipAddr_toString(&ip,ipstr);
            printf("From: %s\n",ipstr);
            //print_buff(buffer,n);
            dns_decoded_t  resp[5000];
            size_t respSize=sizeof(resp);
            dns_rcode_t r=dns_decode(resp,&(respSize),(dns_packet_t*)buffer,n);
            printf("-----------------------\n");
            dns_print_result((dns_query_t *)resp);
            printf("-----------------------\n");
        }
        sendQuery();
        printf("sleeping..\n");
        sleep(5);
    }
    return 0;
}
