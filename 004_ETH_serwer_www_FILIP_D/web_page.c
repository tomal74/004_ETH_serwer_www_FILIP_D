/*
 * web_page.c
 *
 *  Created on: 20 mar 2020
 *      Author: G505s
 */


#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include "ip_arp_udp_tcp.h"
#include "enc28j60.h"
#include "websrv_help_functions.h"
#include "net.h"

#include "web_page.h"
#include "main.h"

uint8_t buf[BUFFER_SIZE+1];
// -------- never change anything below this line ---------
// global string buffer
#define STR_BUFFER_SIZE 20
static char gStrbuf[STR_BUFFER_SIZE+1];
uint16_t gPlen;
static char ntp_unix_time_str[11];
uint8_t ntp_client_attempts=0;
uint8_t haveNTPanswer=0;
uint8_t send_ntp_req_from_idle_loop=0;
// this is were we keep time (in unix gmtime format):
// Note: this value may jump a few seconds when a new ntp answer comes.
// You need to keep this in mid if you build an alarm clock. Do not match
// on "==" use ">=" and set a state that indicates if you already triggered the alarm.
volatile uint32_t time=0;
uint32_t on_time_1, on_time_2;
uint32_t off_time_1, off_time_2;
uint32_t wait_time_1, wait_time_2;

uint8_t str_buffer[6];
uint16_t visitor_counter;

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
        plen=fill_tcp_data_p(buf,plen,PSTR("<br><hr>"));
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
	*plen = fill_tcp_data_p(buf, *plen, PSTR("[<script>t2s("));
	*plen = fill_tcp_data(buf, *plen, &ntp_unix_time_str[i]);
	*plen = fill_tcp_data_p(buf, *plen, PSTR(")</script>], "));
}


// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage(uint8_t *buf) {
        uint16_t plen;

        plen=http200ok();
        plen=fill_tcp_data_p(buf,plen,PSTR("<script src=t1.js></script>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<pre>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<hr><img src=https://bit.ly/2WdYsHx width=\"200\" height=\"200\">"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<i>nr odp:"));
        sprintf( (char*)str_buffer, "%d", visitor_counter );
        plen=fill_tcp_data(buf, plen, (char*) str_buffer);
        memset(str_buffer, 0, sizeof(str_buffer));
        plen=fill_tcp_data_p(buf,plen,PSTR("</i>\n\n"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<h1>OBORA: "));

        if(fail_1) plen=fill_tcp_data_p(buf,plen,PSTR("<font color='red'>AWARIA!</font>"));
        else if(!cap_1 && !rly_1 && !fail_1) plen=fill_tcp_data_p(buf,plen,PSTR("Stop")); //stop, czas pracy
        else if( cap_1 && !rly_1 && !fail_1) plen=fill_tcp_data_p(buf,plen,PSTR("Czekam")); //czekam, godzina
        else if( cap_1 &&  rly_1 && !fail_1) plen=fill_tcp_data_p(buf,plen,PSTR("Praca...")); //zaladunek, godzina startu
        else plen=fill_tcp_data_p(buf,plen,PSTR("<font color='red'>---</font>")); //Stan teoretycznie niemozliwy
        //else if(!cap_1 && !rly_1 && fail_1)  //awaria, rura pelna
        //else if(cap_1 && !rly_1 && fail_1) //awaria, rura pusta
        plen=fill_tcp_data_p(buf,plen,PSTR("</h1><h2>"));
        printUNIXtime(on_time_1, &plen);
        sprintf( (char*)str_buffer, "%ld", off_time_1 );
        plen=fill_tcp_data(buf, plen, (char*) str_buffer);
		memset(str_buffer, 0, sizeof(str_buffer));
        plen=fill_tcp_data_p(buf,plen,PSTR("min</h2>"));

        plen=fill_tcp_data_p(buf,plen,PSTR("<h1></h1>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<hr>"));
        plen=fill_tcp_data_p(buf,plen,PSTR("<h1>STODOLA: "));

        if(fail_2) plen=fill_tcp_data_p(buf,plen,PSTR("<font color='red'>AWARIA!</font>"));
        else if(!cap_2 && !rly_2 && !fail_2) plen=fill_tcp_data_p(buf,plen,PSTR("Stop")); //stop, czas pracy
        else if( cap_2 && !rly_2 && !fail_2) plen=fill_tcp_data_p(buf,plen,PSTR("Czekam")); //czekam, godzina
        else if( cap_2 &&  rly_2 && !fail_2) plen=fill_tcp_data_p(buf,plen,PSTR("Praca..")); //zaladunek, godzina startu
        else plen=fill_tcp_data_p(buf,plen,PSTR("<font color='red'>---</font>")); //Stan teoretycznie niemozliwy
        //else if(!cap_2 && !rly_2 && fail_2)  //awaria, rura pelna
        //else if(cap_2 && !rly_2 && fail_2) //awaria, rura pusta
        plen=fill_tcp_data_p(buf,plen,PSTR("</h1><h2>"));
        printUNIXtime(on_time_2, &plen);
        sprintf( (char*)str_buffer, "%ld", off_time_2 );
        plen=fill_tcp_data(buf, plen, (char*) str_buffer);
		memset(str_buffer, 0, sizeof(str_buffer));
        plen=fill_tcp_data_p(buf,plen,PSTR("min</h2>"));

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
