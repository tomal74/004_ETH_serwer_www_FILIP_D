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
#include "ip_arp_udp_tcp.h"
#include "enc28j60.h"
#include "websrv_help_functions.h"
#include <util/delay.h>
#include "net.h"

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
static uint8_t ntpip[4] = {134,130,4,17};
//132.163.4.103
// server listen port for www
#define MYWWWPORT 881

// -------- never change anything below this line ---------
// global string buffer
#define STR_BUFFER_SIZE 20
static char gStrbuf[STR_BUFFER_SIZE+1];
static uint16_t gPlen;

static char ntp_unix_time_str[11];
static uint8_t ntpclientportL=0; // lower 8 bytes of local port number
static uint8_t ntp_client_attempts=0;
static uint8_t haveNTPanswer=0;
static uint8_t send_ntp_req_from_idle_loop=0;
// this is were we keep time (in unix gmtime format):
// Note: this value may jump a few seconds when a new ntp answer comes.
// You need to keep this in mid if you build an alarm clock. Do not match
// on "==" use ">=" and set a state that indicates if you already triggered the alarm.
volatile uint32_t time=0;
#define GETTIMEOFDAY_TO_NTP_OFFSET 2208988800UL

#define BUFFER_SIZE 900
static uint8_t buf[BUFFER_SIZE+1];

// dioda LED
#define LED_DDR DDRC
#define LED_PIN (1<<PC4)			// definicja pinu do którego pod³¹czona jest dioda
#define LED_TOG PORTC ^= LED_PIN	// makrodefinicja – zmiana stanu diody
#define LED_OFF PORTC |= LED_PIN
#define LED_ON PORTC &= ~LED_PIN

#define CAPSENS1_PORT PORTD
#define CAPSENS1_PIN PIND
#define CAPSENS1_DDR DDRD
#define CAPSENS1_MASK (1<<PD3)

#define CAPSENS2_PORT PORTD
#define CAPSENS2_PIN PIND
#define CAPSENS2_DDR DDRD
#define CAPSENS2_MASK (1<<PD4)

#define MAIN_RELAY1_PORT PORTD
#define MAIN_RELAY1_PIN PIND
#define MAIN_RELAY1_DDR DDRD
#define MAIN_RELAY1_MASK (1<<PD5)

#define MAIN_RELAY2_PORT PORTD
#define MAIN_RELAY2_PIN PIND
#define MAIN_RELAY2_DDR DDRD
#define MAIN_RELAY2_MASK (1<<PD6)

#define FAILURE1_PORT PORTD
#define FAILURE1_PIN PIND
#define FAILURE1_DDR DDRD
#define FAILURE1_MASK (1<<PD7)

#define FAILURE2_PORT PORTB
#define FAILURE2_PIN PINB
#define FAILURE2_DDR DDRB
#define FAILURE2_MASK (1<<PB0)
/* Time To Get Time From NTP Server */
//#define SEC_TO_SYNC 65500UL	// Aprox 18h
//#define SEC_TO_SYNC 3600UL		// 1h
#define SEC_TO_SYNC 9720UL


volatile uint8_t Timer1, Timer2, Timer3, Timer4, Timer5, Timer6, Timer7;
uint8_t cap_1, cap_2, rly_1, rly_2, fail_1, fail_2;

uint16_t visitor_counter;
uint8_t str_buffer[6];
volatile uint16_t time_to_sync=10;

static uint32_t on_time_1;
static uint32_t on_time_2;

const char http_head[] PROGMEM = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n";
const char tekst_beg[] PROGMEM = "<font size='5'>";
const char r_tekst_beg[] PROGMEM = "<font color='red' size='5'>";
const char g_tekst_beg[] PROGMEM = "<font color='green' size='5'>";
const char tekst_end[] PROGMEM = "\n</font>";

uint16_t http200ok(void)
{
        return(fill_tcp_data_p(buf,0,http_head));
}


uint16_t http200okjs(void)
{
        return(fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: application/x-javascript\r\n\r\n")));
}

// t1.js
uint16_t print_t1js(void)
{
        uint16_t plen;
        plen=http200okjs();
        // unix time to printable string
        plen=fill_tcp_data_p(buf,plen,PSTR("\
function t2s(n){\n\
var t = new Date(n*1000);\n\
document.write(t.toLocaleString());\n\
}\n\
"));
        return(plen);
}

uint16_t print_webpage_ok(void)
{
        uint16_t plen;
        plen=http200ok();
        plen=fill_tcp_data_p(buf,plen,PSTR("<a href=/>OK</a>"));
        return(plen);
}


// prepare the main webpage by writing the data to the tcp send buffer
uint16_t t_print_webpage(void)
{
        uint16_t plen;
        uint8_t i;
        uint32_t temp_time = time;
        plen=http200ok();
        plen=fill_tcp_data_p(buf,plen,PSTR("<a href=/m>[new ntp query]</a> "));
        plen=fill_tcp_data_p(buf,plen,PSTR("<script src=t1.js></script>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<h2>NTP response status</h2>\n<pre>\n"));
        // convert number to string:
        plen=fill_tcp_data_p(buf,plen,PSTR("\nGW mac known: "));
        if (client_waiting_gw()==0){
                plen=fill_tcp_data_p(buf,plen,PSTR("yes"));
        }else{
                plen=fill_tcp_data_p(buf,plen,PSTR("no"));
        }
        plen=fill_tcp_data_p(buf,plen,PSTR("\nNumber of requests: "));
        // convert number to string:
        itoa(ntp_client_attempts,gStrbuf,10);
        plen=fill_tcp_data(buf,plen,gStrbuf);
        plen=fill_tcp_data_p(buf,plen,PSTR("\nSuccessful answers: "));
        // convert number to string:
        itoa(haveNTPanswer,gStrbuf,10);
        plen=fill_tcp_data(buf,plen,gStrbuf);
        plen=fill_tcp_data_p(buf,plen,PSTR("\n\nTime received in UNIX sec since 1970:\n</pre>"));
        if (haveNTPanswer) {
                i=10;
                // convert a 32bit integer into a printable string, avr-lib has
                // not function for that therefore we do it step by step:
                ntp_unix_time_str[i]='\0';
                while(temp_time&&i){
                        i--;
                        ntp_unix_time_str[i]=(char)((temp_time%10 & 0xf))+0x30;
                        temp_time=temp_time/10;
                }
                plen=fill_tcp_data(buf,plen,&ntp_unix_time_str[i]);
                plen=fill_tcp_data_p(buf,plen,PSTR(" [<script>t2s("));
                plen=fill_tcp_data(buf,plen,&ntp_unix_time_str[i]);
                plen=fill_tcp_data_p(buf,plen,PSTR(")</script>]"));
        }else{
                plen=fill_tcp_data_p(buf,plen,PSTR("none"));
        }
        plen=fill_tcp_data_p(buf,plen,PSTR("<br><hr>Tomasz K"));
        return(plen);
}


void printUNIXtime(uint32_t temp_time, uint16_t *plen) {
    uint8_t i;

	i = 10;
	// convert a 32bit integer into a printable string, avr-lib has
	// not function for that therefore we do it step by step:
	ntp_unix_time_str[i] = '\0';
	while (temp_time && i) {
		i--;
		ntp_unix_time_str[i] = (char) ((temp_time % 10 & 0xf)) + 0x30;
		temp_time = temp_time / 10;
	}
	*plen = fill_tcp_data_p(buf, *plen, PSTR("  [<script>t2s("));
	*plen = fill_tcp_data(buf, *plen, &ntp_unix_time_str[i]);
	*plen = fill_tcp_data_p(buf, *plen, PSTR(")</script>]"));
}


// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage(uint8_t *buf) {
        uint16_t plen;

        plen=http200ok();
        plen=fill_tcp_data_p(buf,plen,PSTR("<script src=t1.js></script>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<pre>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<hr><img src=https://bit.ly/2WdYsHx width=\"200\" height=\"200\">"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<font><i>nr odp:"));
        sprintf( (char*)str_buffer, "%d", visitor_counter );
        plen=fill_tcp_data(buf, plen, (char*) str_buffer);
        plen=fill_tcp_data_p(buf,plen,PSTR("</i>\n\n</font>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<h1>OBORA:</h1>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("\n"));
        plen=fill_tcp_data_p(buf,plen,tekst_beg);
        switch(rly_1) {
        	case 0:
        		plen=fill_tcp_data_p(buf,plen,PSTR("Czekam"));
        		break;
        	case 1:
        		plen=fill_tcp_data_p(buf,plen,PSTR("Praca"));
        		break;
        }
        printUNIXtime(on_time_1, &plen);
        plen=fill_tcp_data_p(buf,plen,tekst_end);
        plen=fill_tcp_data_p(buf,plen,tekst_beg);
        switch(cap_1) {
        	case 0:
        		plen=fill_tcp_data_p(buf,plen,PSTR("Rura Pelna"));
        		break;
        	case 1:
        		plen=fill_tcp_data_p(buf,plen,PSTR("Rura Pusta"));
        		break;
        }
        plen=fill_tcp_data_p(buf,plen,tekst_end);
        switch(fail_1) {
            case 0:
                plen=fill_tcp_data_p(buf,plen,g_tekst_beg);
                plen=fill_tcp_data_p(buf,plen,PSTR("OK"));
                break;
            case 1:
                plen=fill_tcp_data_p(buf,plen,r_tekst_beg);
               	plen=fill_tcp_data_p(buf,plen,PSTR("AWARIA"));
                break;
               }
        plen=fill_tcp_data_p(buf,plen,tekst_end);
        plen=fill_tcp_data_p(buf,plen,PSTR("<h1></h1>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<hr>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<h1>STODOLA:\n</h1>"));
        plen=fill_tcp_data_p(buf,plen,tekst_beg);
        switch(rly_2) {
                	case 0:
                		plen=fill_tcp_data_p(buf,plen,PSTR("Czekam"));
                		break;
                	case 1:
                		plen=fill_tcp_data_p(buf,plen,PSTR("Praca"));
                		break;
                }
        printUNIXtime(on_time_2, &plen);
        plen=fill_tcp_data_p(buf,plen,tekst_end);
        plen=fill_tcp_data_p(buf,plen,tekst_beg);
        switch(cap_2) {
        	case 0:
        		plen=fill_tcp_data_p(buf,plen,PSTR("Rura Pelna"));
        		break;
        	case 1:
        		plen=fill_tcp_data_p(buf,plen,PSTR("Rura Pusta"));
        		break;
        }
        plen=fill_tcp_data_p(buf,plen,tekst_end);
        switch(fail_2) {
        	case 0:
        		plen=fill_tcp_data_p(buf,plen,g_tekst_beg);
        		plen=fill_tcp_data_p(buf,plen,PSTR("OK"));
        		break;
        	case 1:
        		plen=fill_tcp_data_p(buf,plen,r_tekst_beg);
        		plen=fill_tcp_data_p(buf,plen,PSTR("AWARIA"));
        		break;
        }
        plen=fill_tcp_data_p(buf,plen,tekst_end);
        plen=fill_tcp_data_p(buf,plen,PSTR("</pre>\n"));
        return(plen);
}


uint16_t print_webpage_ntp_req(void)
{
        uint16_t plen;
        plen=http200ok();
        plen=fill_tcp_data_p(buf,plen,PSTR("<a href=/>[home]</a>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<h2>send ntp request</h2><pre>\n"));
        plen=fill_tcp_data_p(buf,plen,PSTR("ntp server: "));
        mk_net_str(gStrbuf,ntpip,4,'.',10);
        plen=fill_tcp_data(buf,plen,gStrbuf);
        plen=fill_tcp_data_p(buf,plen,PSTR("<form action=/r method=get>\n"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<input type=submit value=\"send\">\n"));
        plen=fill_tcp_data_p(buf,plen,PSTR("</form>\n"));
        plen=fill_tcp_data_p(buf,plen,PSTR("</pre><br><hr>Tomasz K"));
        return(plen);
}


// analyse the url given
// return values: are used to show different pages (see main program)
//
int8_t analyse_get_url(char *str) {
	// the first slash:
	if (str[0] == '/' && str[1] == ' ') {
		// end of url, display just the web page
		return (0);
	}
	if (strncmp("/flr ", str, 5) == 0) {
		gPlen = print_webpage(buf);
    	visitor_counter++;
		return (10);
	}
    if (strncmp("/t1.js",str,6)==0){
            gPlen=print_t1js();
            return(10);
    }
	if (strncmp("/m ", str, 3) == 0) {
		gPlen = print_webpage_ntp_req();
		return (10);
	}
	if (strncmp("/r? ", str, 4) == 0) {
		// we can't call here client_ntp_request as this would mess up the buf variable
		// for our currently worked on web-page.
		send_ntp_req_from_idle_loop = 1;
		return (1);
	}
	return (-1); // Unauthorized
}

void io_init(void) {
	/* IO pins as inputs, pull-up with internal resistors to Vcc */
	CAPSENS1_DDR &= ~CAPSENS1_MASK;
	CAPSENS1_PORT |= CAPSENS1_MASK;
	CAPSENS2_DDR &= ~CAPSENS2_MASK;
	CAPSENS2_PORT |= CAPSENS2_MASK;

	MAIN_RELAY1_DDR &= ~MAIN_RELAY1_MASK;
	MAIN_RELAY1_PORT |= MAIN_RELAY1_MASK;
	MAIN_RELAY2_DDR &= ~MAIN_RELAY2_MASK;
	MAIN_RELAY2_PORT |= MAIN_RELAY2_MASK;

	FAILURE1_DDR &= ~FAILURE1_MASK;
	FAILURE1_PORT |= FAILURE1_MASK;
	FAILURE2_DDR &= ~FAILURE2_MASK;
	FAILURE2_PORT |= FAILURE2_MASK;

	/* LED pin as output - kathode to uC */
	LED_DDR |= LED_PIN;
	/* Switch off the LED */
	LED_OFF;
}


void ctc_tim0_init(void) {
	/* CTC mode */
	TCCR0A |= (1<<WGM01);
	/* Pescaler = 1024 */
	TCCR0B |= (1<<CS00) | (1<<CS02);
	/* Compare Match A Interrupt Enable */
	TIMSK0 |= (1<<OCIE0A);
	/* CTC 'A' register = 121 - F_IRQ = 100,058Hz - FCPU From ENC28J60 CLKOUT = 12.5MHz */
	OCR0A = 121;
}


void ctc_tim1_init(void) {
	/* CTC mode */
	TCCR1B |= (1<<WGM12);
	/* Pescaler = 1024 */
	TCCR1B |= (1<<CS10) | (1<<CS12);
	/* Compare Match A Interrupt Enable */
	TIMSK1 |= (1<<OCIE1A);
	/* CTC 'A' register = 12206 - F_IRQ = 1Hz - FCPU From ENC28J60 CLKOUT = 12.5MHz */
	OCR1A = 12206;
}


/************** funkcja RelaySuperDebounce do obs³ugi np.pojedynczych wyjœæ przekaŸników ***************
 * 							AUTOR: Miros³aw Kardaœ
 * 							MODYFIKACJA: Tomasz Konieczka 13.03.2020r
 * 						   (KORONAWIRUS U BRAM... ZAMKNIÊTE SZKO£Y ITD.)
 * ZALETY:
 * 		- nie wprowadza najmniejszego spowalnienia
 * 		- mo¿na przydzieliæ ró¿ne akcje dla trybu klikniêcia
 *
 * Wymagania:
 * 	Timer programowy utworzony w oparciu o Timer sprzêtowy (przerwanie 100Hz)
 *
 * 	Parametry wejœciowe:
 *
 * 	*key_state - wskaŸnik na zmienn¹ w pamiêci RAM (1 bajt) - do przechowywania stanu klawisza
 *  *KPIN - nazwa PINx portu na którym umieszczony jest klawisz, np: PINB
 *  key_mask - maska klawisza np: (1<<PB3)
 *  push_proc - wskaŸnik do w³asnej funkcji wywo³ywanej raz po zwolenieniu przycisku
 *
 *  return:
 *
 *  0 - klawisz niewcisniety
 *  1 - klawisz wciœniêty
 *  2 - zbocze narastajace
 *  3 - zbocze opadajace
 **************************************************************************************/
uint8_t RelaySuperDebounce( uint8_t * key_state, volatile uint8_t *KPIN, uint8_t key_mask,
		volatile uint8_t *soft_timer, void (*push_proc)(void) ) {

	enum {idle, debounce, rising_edge, pressed};

	uint8_t key_press = !(*KPIN & key_mask);

	if( key_press && !*key_state ) {
		*key_state = debounce;
		*soft_timer = 30;
	}
	else if( *key_state  ) {
		if( key_press && debounce==*key_state && !*soft_timer ) {
			*key_state = rising_edge;
			*soft_timer = 30;
		}
		else if( key_press && rising_edge==*key_state && !*soft_timer ) {
			*key_state = pressed;
			return 2;
		}
		else if( key_press && pressed==*key_state && !*soft_timer ) {
			return 1;
		}
		else if( !key_press && *key_state>1 ) {
			if(push_proc) push_proc();						/* KEY_UP */
			*key_state=idle;
			return 3;
		}
	}
	return 0;
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

		if(rly_1 == 2) {
			on_time_1 = time;
		}
		/* Rising Edge Of Cap Sensor2 - Get Start Time Of Second Relay */
		if(rly_2 == 2) {
			on_time_2 = time;
		}
		/* Set Interval Beetwen State Checking As 1s */
		Timer1 = 100;
	}
}

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
}

/* interrupt, step seconds counter */
ISR(TIMER1_COMPA_vect) {
	LED_TOG;
	time++;
	if(time_to_sync) time_to_sync--;
}
