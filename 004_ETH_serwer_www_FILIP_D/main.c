/*********************************************
 * vim:sw=8:ts=8:si:et
 * To use the above modeline in vim you must have "set modeline" in your .vimrc
 * Author: Guido Socher
 * Copyright: GPL V2
 *
 * Tuxgraphics AVR webserver/ethernet board
 *
 * http://tuxgraphics.org/electronics/
 * Chip type           : Atmega88/168/328 with ENC28J60
 *
 *
 * MODYFIKACJE: Miros³aw Kardaœ --- ATmega32
 * MODYFIKACJE: Tomasz Konieczka --- Atmega328p
 *
 *********************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "ip_arp_udp_tcp.h"
#include "enc28j60.h"
#include "websrv_help_functions.h"
#include "net.h"
#include "fun.h"
#include "web_page.h"
#include "main.h"


// ustalamy adres MAC
static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x29};
// ustalamy adres IP urz¹dzenia
static uint8_t myip[4] = {192,168,1,110};
// Default gateway. The ip address of your DSL router. It can be set to the same as
// msrvip the case where there is no default GW to access the
// web server (=web server is on the same lan as this host)
static uint8_t gwip[4] = {192,168,1,1};
// --------------- normally you don't change anything below this line
// time.apple.com (any of 17.254.0.31, 17.254.0.26, 17.254.0.27, 17.254.0.28):
//static uint8_t ntpip[4] = {132,163,4,103};
uint8_t ntpip[4] = {134,130,4,17};
//132.163.4.103
// server listen port for www
#define MYWWWPORT 881

/* Time To Get Time From NTP Server */
//#define SEC_TO_SYNC 65500UL	// Aprox 18h
//#define SEC_TO_SYNC 3600UL		// 1h
#define SEC_TO_SYNC 9720UL
#define GETTIMEOFDAY_TO_NTP_OFFSET 2208988800UL
static uint8_t ntpclientportL=0; // lower 8 bytes of local port number
volatile uint8_t Timer1, Timer2, Timer3, Timer4, Timer5, Timer6, Timer7;
uint8_t cap_1, cap_2, rly_1, rly_2, fail_1, fail_2;
volatile uint16_t time_to_sync=10;


void debouncer_process(void);


/* User Main Begin */
int main(void) {

	//uint16_t dat_p;
	int8_t cmd;
	uint16_t pktlen;

	// set the clock speed to 8MHz
	// set the clock prescaler. First write CLKPCE to enable setting of clock the
	// next four instructions.
	CLKPR = (1 << CLKPCE);
	CLKPR = 0; // 8 MHZ
	_delay_loop_1(0); // 60us

	//initialize the hardware driver for the enc28j60
	enc28j60Init(mymac);
	enc28j60clkout(2); // change clkout from 6.25MHz to 12.5MHz
	_delay_loop_1(0); // 60us
	enc28j60PhyWrite(PHLCON, 0x476);

	//init the ethernet/ip layer:
	init_ip_arp_udp_tcp(mymac, myip, MYWWWPORT);
	// init the web client:
	client_set_gwip(gwip);  // e.g internal IP of dsl router

	/* init timer0 at CTC mode, f=100Hz - IRQ every 10ms */
	ctc_tim0_init();
	/* init 16-bit timer1 at CTC mode, f=1Hz - IRQ every 1000ms */
	ctc_tim1_init();

	_delay_ms(20);
	/* init IO pins */
	io_init();

	/* Blink LED One Time After Reset */
	LED_ON;
	_delay_ms(200);
	LED_OFF;

	sei();

	/* ( XXX Be Careful On Time ) Enable Watchdog With Max Reset Time = 8s */
	wdt_enable(WDTO_8S);

	/* User While Begin */
	while (1) {
		/* ( XXX Be Careful On Time ) Watchdog Reset */
		wdt_reset();

		/* Check State On Every Input Pin */
		debouncer_process();
		/* Rising Edge Of Cap Sensor1 - Get Start Time Of First Relay */
		/* Time Need To Be Synchronize */
		if(!time_to_sync) {
			/* Synchronize Time - NTP */
			send_ntp_req_from_idle_loop=1;
			/* Set Next Synchro Time */
			time_to_sync = SEC_TO_SYNC;
		}

		// read packet, handle ping and wait for a tcp packet:
		//dat_p = packetloop_icmp_tcp(buf, enc28j60PacketReceive(BUFFER_SIZE, buf));
        pktlen=enc28j60PacketReceive(BUFFER_SIZE, buf);
        // handle ping and wait for a tcp packet
        gPlen=packetloop_icmp_tcp(buf,pktlen);

		/* dat_p will be unequal to zero if there is a valid http get */
		if (gPlen == 0) {
			// no http request
			goto UDP;
		}
		// tcp port 80 begin
		if (strncmp("GET ", (char *) &(buf[gPlen]), 4) != 0) {
			// head, post and other methods:
			gPlen = http200ok();
			gPlen = fill_tcp_data_p(buf, gPlen, PSTR("<h1>200 OK</h1>"));
			goto SENDTCP;
		}
		/* Choice Correct Page */
        cmd=analyse_get_url((char *)&(buf[gPlen+4]));
        // for possible status codes see:
        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
        if (cmd==-1){
             gPlen=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Unauthorized</h1>"));
             goto SENDTCP;
        }
        if (cmd==1){
            gPlen=print_webpage_ok();
            goto SENDTCP;
        }
        if (cmd==10){
            goto SENDTCP;
        }
         gPlen=t_print_webpage();


SENDTCP:
		LED_ON;
		www_server_reply(buf, gPlen); // send web page data
		// tcp port 80 end
		LED_OFF;
		continue;
        // tcp port www end --- udp start
UDP:
        // check if ip packets are for us:
        if(eth_type_is_ip_and_my_ip(buf,pktlen)==0){
                // we are idle here:
                if (send_ntp_req_from_idle_loop && client_waiting_gw()==0){
                       // LEDON;
                        ntpclientportL++; // new src port
                        send_ntp_req_from_idle_loop=0;
                        client_ntp_request(buf,ntpip,ntpclientportL);
                        ntp_client_attempts++;
                }
                continue;
        }
        // ntp src port must be 123 (0x7b):
        if (buf[IP_PROTO_P]==IP_PROTO_UDP_V&&buf[UDP_SRC_PORT_H_P]==0&&buf[UDP_SRC_PORT_L_P]==0x7b){
                if (client_ntp_process_answer(buf,&time,ntpclientportL)){
                      //  LEDOFF;
                        // convert to unix time:
                        if ((time & 0x80000000UL) ==0){
                                // 7-Feb-2036 @ 06:28:16 UTC it will wrap:
                                time+=2085978496;
                        }else{
                                // from now until 2036:
                                time-=GETTIMEOFDAY_TO_NTP_OFFSET;
                        }
                        haveNTPanswer++;
                }
        }
	} 	/* User While End */
	return (0);
}	/* User Main End */




ISR(TIMER0_COMPA_vect) {	/* Timer0 CTC IRQ - F=100Hz, T=10ms */
	static uint8_t led_blinker_tim;
	uint8_t n;

	n = Timer1;
	if(n) Timer1 = --n;

	n = Timer2;
	if(n) Timer2 = --n;

	n = Timer3;
	if(n) Timer3 = --n;

	n = Timer4;
	if(n) Timer4 = --n;

	n = Timer5;
	if(n) Timer5 = --n;

	n = Timer6;
	if(n) Timer6 = --n;

	n = Timer7;
	if(n) Timer7 = --n;

	if(++led_blinker_tim == 60) LED_ON;
	else if(led_blinker_tim == 65) { LED_OFF; led_blinker_tim=0; }
}

/* interrupt, step seconds counter */
ISR(TIMER1_COMPA_vect) {
	//LED_TOG;
	time++;
	if(time_to_sync) time_to_sync--;
}


void debouncer_process(void) {
    /* Auxiliary variables to switches */
   static uint8_t sw1, sw2, sw3, sw4, sw5, sw6;

	if(!Timer1) {
	    /* Check State On Every Input Pin */
		cap_1 =  RelaySuperDebounce(&sw1, &CAPSENS1_PIN, CAPSENS1_MASK, &Timer2, NULL);
	    cap_2 =  RelaySuperDebounce(&sw2, &CAPSENS2_PIN, CAPSENS2_MASK, &Timer3, NULL);
		rly_1 =  RelaySuperDebounce(&sw3, &MAIN_RELAY1_PIN, MAIN_RELAY1_MASK, &Timer4, NULL);
		rly_2 =  RelaySuperDebounce(&sw4, &MAIN_RELAY2_PIN, MAIN_RELAY2_MASK, &Timer5, NULL);
		fail_1 = RelaySuperDebounce(&sw5, &FAILURE1_PIN, FAILURE1_MASK, &Timer6, NULL);
		fail_2 = RelaySuperDebounce(&sw6, &FAILURE2_PIN, FAILURE2_MASK, &Timer7, NULL);

		/* Main Relay1 On - Get Work Time Of First Relay */
		if(rly_1 == 1) { off_time_1 = (time - on_time_1) / 60UL; off_time_s1 = (time - on_time_1) % 60UL; }
		/* Rising Edge Of Cap Sensor1 - Get Start Time Of First Relay */
		else if(rly_1 == 2) on_time_1 = time;
		/* Main Relay2 On - Get Work Time Of Second Relay */
		if(rly_2 == 1) { off_time_2 = (time - on_time_2) / 60UL; off_time_s2 = (time - on_time_2) % 60UL; }
		/* Rising Edge Of Cap Sensor2 - Get Start Time Of Second Relay */
		else if(rly_2 == 2) on_time_2 = time;
		/* Rising Edge On Cap Sensor1 - Get The Wait Start Time */
		if(cap_1 == 2) wait_time_1 = time;
		/* Rising Edge On Cap Sensor2 - Get The Wait Start Time */
		if(cap_2 == 2) wait_time_2 = time;
		/* Rising Edge On Fail Relay1 - Get The Fail Start Time */
		if(fail_1 == 2) fail_time_1 = time;
		/* Rising Edge On Fail Relay2 - Get The Fail Start Time */
		if(fail_2 == 2) fail_time_2 = time;

		/* Set Interval Beetwen State Checking As 1s */
		Timer1 = 100;
	}
}
