/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */
#include "dev/cc2420.h"//elnaz
#include <math.h>
#include "contiki.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-udp-packet.h"
#include "net/rpl/rpl.h"
#include "dev/serial-line.h"
#if CONTIKI_TARGET_Z1
#include "dev/uart0.h"
#else
#include "dev/uart1.h"
#endif
#include "collect-common.h"
#include "collect-view.h"

#include <stdio.h>
#include <string.h>

#define UDP_CLIENT_PORT 8775
#define UDP_SERVER_PORT 5688

#define DEBUG DEBUG_PRINT
#include "net/uip-debug.h"

static struct uip_udp_conn *client_conn;
static uip_ipaddr_t server_ipaddr;

//static uint16_t etx_array[20];
//static uint16_t  variance=0;
//static uint16_t  mean=0;
//static uint8_t scntr=0;
static unsigned long avg=0;
static unsigned long std=0;
static uint8_t cntr=0;

static char flag_etx=0;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&udp_client_process, &collect_common_process);
/*---------------------------------------------------------------------------*/
void
collect_common_set_sink(void)
{
  /* A udp client can never become sink */
}
/*---------------------------------------------------------------------------*/

void
collect_common_net_print(void)
{
  rpl_dag_t *dag;
  uip_ds6_route_t *r;

  /* Let's suppose we have only one instance */
  dag = rpl_get_any_dag();
  if(dag->preferred_parent != NULL) {
    PRINTF("Preferred parent: ");
    PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
    PRINTF("\n");
  }
  for(r = uip_ds6_route_head();
      r != NULL;
      r = uip_ds6_route_next(r)) {
    PRINT6ADDR(&r->ipaddr);
  }
  PRINTF("---\n");
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  if(uip_newdata()) {
    /* Ignore incoming data */
  }
}
/*---------------------------------------------------------------------------*/
void
collect_common_send(void)
{
//elnaz
unsigned long delta;
//elnaz
  static uint8_t seqno;
  struct {
    uint8_t seqno;
    uint8_t for_alignment;
    struct collect_view_data_msg msg;
  } msg;
  /* struct collect_neighbor *n; */
  uint16_t parent_etx,temp;
  uint16_t rtmetric;
  uint16_t num_neighbors;
  uint16_t beacon_interval;
  rpl_parent_t *preferred_parent;
  rimeaddr_t parent;
  rpl_dag_t *dag;
static uint8_t i; 
//elnaz
unsigned long time;

 rimeaddr_t *originator;
uint8_t hops=0;
//elnaz


  if(client_conn == NULL) {
    /* Not setup yet */
    return;
  }
  memset(&msg, 0, sizeof(msg));
  seqno++;
  if(seqno == 0) {
    /* Wrap to 128 to identify restarts */
    seqno = 128;
  }
  msg.seqno = seqno;

  rimeaddr_copy(&parent, &rimeaddr_null);
  parent_etx = 0;

  /* Let's suppose we have only one instance */
  dag = rpl_get_any_dag();
  if(dag != NULL) {
    preferred_parent = dag->preferred_parent;
    if(preferred_parent != NULL) {
      uip_ds6_nbr_t *nbr;
      nbr = uip_ds6_nbr_lookup(rpl_get_parent_ipaddr(preferred_parent));
      if(nbr != NULL) {
        /* Use parts of the IPv6 address as the parent address, in reversed byte order. */
        parent.u8[RIMEADDR_SIZE - 1] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 2];
        parent.u8[RIMEADDR_SIZE - 2] = nbr->ipaddr.u8[sizeof(uip_ipaddr_t) - 1];
        parent_etx = (preferred_parent->link_metric);// /16;
      }
    }
    rtmetric = dag->rank;
    beacon_interval = (uint16_t) ((2L << dag->instance->dio_intcurrent) / 1000);
    num_neighbors = RPL_PARENT_COUNT(dag);
  } else {
    rtmetric = 0;
    beacon_interval = 0;
    num_neighbors = 0;
  }
//elnaz
//i= (seqno%20);
if(seqno>10)
	{flag_etx=1;}
/*
if(flag_etx==1) // the statistics are calculated between the seqno>10 and timer expires
{
	scntr++;
temp= parent_etx/64;
delta= parent_etx - mean;
mean = mean + (delta/scntr);
variance = variance + delta*((parent_etx)-mean);

	avg = avg + parent_etx ;
	std = std + (parent_etx/32)^2;
	
}*/
if(flag_etx==1)
{
	/*
	if(cntr==20)
		{	cntr=0;}*/
  	avg = avg+parent_etx;
	cntr++;  
}

//elnaz
  /* num_neighbors = collect_neighbor_list_num(&tc.neighbor_list); */
  collect_view_construct_message(&msg.msg, &parent,
                                 parent_etx, rtmetric,
                                 num_neighbors, beacon_interval);

  uip_udp_packet_sendto(client_conn, &msg, sizeof(msg),
                        &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));




rimeaddr_copy(&originator, &uip_lladdr);


  printf(" %u",msg.msg.len+8);

  time = clock_seconds();
  printf(" %lu %lu 0", ((time >> 16) & 0xffff), time & 0xffff);
 
  printf(" %u %u %u %u",
         originator->u8[0] + (originator->u8[1] << 8), seqno, hops, 0);


  printf(" %u",msg.msg.len);
  printf(" %u",msg.msg.clock);
  printf(" %u",msg.msg.timesynch_time);
  printf(" %u",msg.msg.cpu);
  printf(" %u",msg.msg.lpm);
  printf(" %u",msg.msg.transmit);
  printf(" %u",msg.msg.listen);
  printf(" %u", msg.msg.parent);
  printf(" %u", msg.msg.parent_etx);
  printf(" %u", msg.msg.current_rtmetric);
  printf(" %u", msg.msg.num_neighbors);
  printf(" %u", msg.msg.beacon_interval);
  for(i=0 ; i<10 ; i++)
	{ printf(" %u", msg.msg.sensors[i]);}
  printf("\n");

//index++;
}
/*---------------------------------------------------------------------------*/
void
collect_common_net_init(void)
{
#if CONTIKI_TARGET_Z1
  uart0_set_input(serial_line_input_byte);
#else
  uart1_set_input(serial_line_input_byte);
#endif
  serial_line_init();
}
/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
int i;
  uint8_t state;

  PRINTF("Client IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  /* set server addressbiaa be ham etminaan bedim ke hatta agar kaamel baa ham dust shodim  */
  uip_ip6addr(&server_ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 1);

}
/*----------------------------------------------------------------------------*/
static void print_stats(void)
{
avg = avg/cntr;
printf("ETX avg=%lu.%lu", avg/128, avg%128 );

/*unsigned long lsum=0;
unsigned long lavg=0;
unsigned long lstd=0;
uint8_t i;
avg = avg /scntr;
std = std/scntr;
std = std - (avg/32)^2;
std = std/scntr;*/
/*for(i=0 ; i<20; i++)
	{printf("etx=%u", etx_array[i]);}*/

/*printf("ETX scntr=%u, avg=%lu.%lu  std=%lu.%lu\n variance=%lu.%lu mean=%lu.lu", scntr, avg/128,avg%128, std%128, std%128,variance/128, variance%128, mean/128, mean%128 );*/

/*variance = variance/(scntr-1);

printf("ETX scntr=%u variance=%lu.%lu mean=%lu.%lu", scntr, variance/4, variance%4, mean/4, mean%4 );*/


/*for(i=0 ; i<20; i++)

		{	//printf("%u ", etx_array[i]);
		lsum = lsum + etx_array[i];
		}
	lavg = (lsum/20);

	lsum=0;
	for(i=0 ; i<20; i++)
		{	lsum =lsum + ((lavg-(etx_array[i]))^2);
		}
	lstd= (lsum/20);

	printf("\nETX: avg=%lu.%lu  std=%lu\n", lavg/128,lavg%128, lstd );*/


}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{

  static struct etimer periodic;


  PROCESS_BEGIN();
cc2420_set_txpower(11);
  PROCESS_PAUSE();

  set_global_address();

  PRINTF("UDP client process started\n");

  print_local_addresses();

  /* new connection with remote host */
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL);
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT));

  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
        UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));


 etimer_set(&periodic, 20*60*CLOCK_SECOND);
  while(1) {//!etimer_expired(&periodic))
    PROCESS_YIELD();

    if(ev == tcpip_event) {
      tcpip_handler();
    }
  if(etimer_expired(&periodic))
	{
	etimer_stop(&periodic);
	flag_etx=0;
	print_stats();
	
	}

  }


	


	
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
