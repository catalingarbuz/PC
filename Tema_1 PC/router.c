#include <queue.h>
#include "skel.h"

#define IP_OFF (sizeof(struct ether_header))
#define ICMP_OFF (IP_OFF + sizeof(struct iphdr))

struct route_table_entry *rtable;
int rtable_size;

struct arp_entry *arp_table;
int arp_table_len = 0;
int interfaces[ROUTER_NUM_INTERFACES];

queue waiting_packets;

//Functie care trimite un singur pachet
void send_one_packet(packet m) {

	struct ether_header *eth_hdr = (struct ether_header *)m.payload;
    struct iphdr *ip_hdr = (struct iphdr *)(m.payload + IP_OFF);
    uint16_t aux_check;
    
    if (ip_hdr->ttl <= 1) {
    	send_icmp_error(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost,
    	                eth_hdr->ether_dhost, ICMP_TIME_EXCEEDED, 0, 
    	                m.interface);
    	return;
    } 

    aux_check = ip_hdr->check;
    ip_hdr->check = 0;
    if (ip_checksum(ip_hdr, sizeof(struct iphdr)) != aux_check) {
    	return;
    }

    ip_hdr->ttl--;
    if (ip_hdr->ttl > 1) {
       ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));
    } else {
      send_icmp_error(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost,
                eth_hdr->ether_dhost, ICMP_TIME_EXCEEDED, 0, m.interface);
      return;
    }
    
    struct route_table_entry *best_route = get_best_route(ip_hdr->daddr);
    if (best_route == NULL) {
    	send_icmp_error(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost, 
    		  eth_hdr->ether_dhost, ICMP_DEST_UNREACH, 0, m.interface);
    	return;
    }

    char* routerip = get_interface_ip(m.interface);
    uint32_t ip_rt = inet_addr(routerip);

    if (ip_hdr->daddr == ip_rt) {
    	send_icmp(ip_hdr->daddr, ip_hdr->saddr, eth_hdr->ether_shost, eth_hdr->ether_dhost, 
      	        ICMP_ECHOREPLY, 0, m.interface, 
      	        htons(getpid() & 0xFFFF), htons((ip_hdr->ttl) & 0xFFFF));
    	return;
    }

    struct arp_entry *arpEntry = get_arp_entry(best_route->next_hop); 
    if (arpEntry == NULL) {
    	return;
    }
    memcpy(eth_hdr->ether_dhost, arpEntry->mac, sizeof(arpEntry->mac));
    get_interface_mac(best_route->interface, eth_hdr->ether_shost);
    
    int send_r = send_packet(best_route->interface, &m);

    DIE(send_r < 0, "Error while sending the packet!");



}

//functie care trimite un pachet arp cu destinatia broadcast
void arp_send(packet m, struct route_table_entry *best_route) {
	 struct ether_header *eth_hdr = (struct ether_header *)m.payload;
     char* srcip_char = get_interface_ip(best_route->interface);
     uint32_t ip_s = inet_addr(srcip_char);
     get_interface_mac(best_route->interface, eth_hdr->ether_shost);
     eth_hdr->ether_type = htons(ETHERTYPE_ARP);
     hwaddr_aton("ff:ff:ff:ff:ff:ff", eth_hdr->ether_dhost);
     send_arp(best_route->next_hop, ip_s, eth_hdr, best_route->interface,
           	           htons(ARPOP_REQUEST));
}

//Functie care trimite toate pachetele din coada q
void send_packets(queue q) {
	while (!queue_empty(q)) {
		void *m_tmp = queue_deq(q);
		packet *m = (packet *) m_tmp;
		send_one_packet(*m);
	}
}

//Functie in care am implementat protocolul ARP
void arp_protocol(packet m) {
	struct ether_header *eth_hdr = (struct ether_header *)m.payload;
	struct arp_header* arp_hdr = parse_arp(m.payload);
	if (arp_hdr != NULL) {
       char* routerip = get_interface_ip(m.interface);
       uint32_t ip_rt = inet_addr(routerip);
       uint8_t mac[6];
       get_interface_mac(m.interface, mac);
       
       if (htons(arp_hdr->op) == ARPOP_REQUEST && arp_hdr->tpa == ip_rt){
          memcpy(eth_hdr->ether_dhost, arp_hdr->sha, 6);
          memcpy(eth_hdr->ether_shost, mac, 6);
          eth_hdr->ether_type = htons(ETHERTYPE_ARP);
          // Trimit pachetul arp cu campurile actualizate
          send_arp(arp_hdr->spa, arp_hdr->tpa, eth_hdr, m.interface,
           	         htons(ARPOP_REPLY));
          return;
         }

        if (htons(arp_hdr->op) == ARPOP_REPLY) {
           // Actualizez arp table
           arp_table[arp_table_len++] = *create_arp_entry(arp_hdr->spa,
        		                                          arp_hdr->sha);
           send_packets(waiting_packets);
         } 
       }
}


int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);

	packet m;
	int rc;
	init(argc - 2, argv + 2);
    rtable = malloc(sizeof(struct route_table_entry) * 70000);
	arp_table = malloc(sizeof(struct  arp_entry) * 100);
	// Citesc tabela de rutare
    read_rtable(argv[1]);
 
	/* Students will write code here */
    waiting_packets = queue_create();

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
	
		// Salvez headere-le pachetului salvat in m
	    struct iphdr *ip_hdr = (struct iphdr *)(m.payload + IP_OFF);
	    
	    if (ip_hdr == NULL) {
	    	continue;
	    }
         

        arp_protocol(m);

        struct route_table_entry *best_route = get_best_route(ip_hdr->daddr);
        
        if (best_route == NULL) {
        	continue;
        } 

        struct arp_entry *arpEntry = get_arp_entry(best_route->next_hop);

        if (arpEntry == NULL) {
        	
        	void* m1 = (packet *) malloc(sizeof(packet));
           	memcpy(m1, &m, sizeof(packet));
            queue_enq(waiting_packets, m1);
            arp_send(m, best_route);
            continue;
        } else 
        	send_one_packet(m);
        }
}
