/*
 * web_page.h
 *
 *  Created on: 20 mar 2020
 *      Author: G505s
 */

#ifndef WEB_PAGE_H_
#define WEB_PAGE_H_


#include <avr/io.h>

#define BUFFER_SIZE 800

extern uint32_t on_time_1, on_time2_1, on_time3_1, on_time_2, on_time2_2, on_time3_2;
extern uint8_t off_time_s1, off_time2_s1, off_time3_s1, off_time_s2, off_time2_s2, off_time3_s2;
extern uint8_t off_time_1, off_time2_1, off_time3_1, off_time_2, off_time2_2, off_time3_2;
extern uint32_t wait_time_1, wait_time_2;
extern uint32_t fail_time_1, fail_time_2;
extern uint8_t send_ntp_req_from_idle_loop;
extern uint8_t buf[BUFFER_SIZE+1];
extern uint16_t gPlen;
extern uint8_t ntp_client_attempts;
extern volatile uint32_t time;
extern uint8_t haveNTPanswer;


uint16_t http200ok(void);
uint16_t http200okjs(void);
uint16_t print_t1js(void);
uint16_t print_webpage_ok(void);
uint16_t t_print_webpage(void);
void printUNIXtime(uint32_t temp_time, uint16_t *plen);
uint16_t print_webpage(uint8_t *buf);
uint16_t print_app_webpage(uint8_t *buf);
uint16_t print_webpage_ntp_req(void);
int8_t analyse_get_url(char *str);


#endif /* WEB_PAGE_H_ */
