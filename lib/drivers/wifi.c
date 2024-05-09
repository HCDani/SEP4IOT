#include "wifi.h"
#include "includes.h"
#include "uart.h"
#include <avrtos\avrtos.h>

#define WIFI_RX_RINGSIZE 16
K_RING_DEFINE(wifi_rx_ring,WIFI_RX_RINGSIZE);
K_SEM_DEFINE(wifi_rx_sem,0,1);

void static wifi_serial_callback(uint8_t received_byte);
WIFI_ERROR_MESSAGE_t wifi_command_with_response(const char *command, char *response, uint8_t maxResponseLength, uint8_t timeOut_s);

void wifi_init() {
    uart_init(USART_WIFI, 115200, wifi_serial_callback);
}

void static wifi_clear_rxring() {
    const uint8_t key = irq_lock();
    k_ring_init(&wifi_rx_ring,wifi_rx_ring_buf,WIFI_RX_RINGSIZE);
    k_sem_take(&wifi_rx_sem,K_NO_WAIT);
    irq_unlock(key);
}

// Runs from ISR
void static wifi_serial_callback(uint8_t received_byte) {
    #ifdef WIFI_DEBUG
//    uart_send_blocking(USART_0,'#');
//    uart_send_blocking(USART_0,received_byte);
    #endif
    k_ring_push(&wifi_rx_ring,received_byte);
    k_sem_give(&wifi_rx_sem);
    k_yield_from_isr();
}

#define WIFI_DATABUFFERSIZE 128
WIFI_ERROR_MESSAGE_t wifi_command(const char *command, uint16_t timeOut_s) {
    char sendbuffer[WIFI_DATABUFFERSIZE];
    
    return wifi_command_with_response(command,sendbuffer,WIFI_DATABUFFERSIZE-1,timeOut_s);
}

WIFI_ERROR_MESSAGE_t wifi_command_AT() {
    return wifi_command("AT", 10);
}

WIFI_ERROR_MESSAGE_t wifi_command_join_AP(char *ssid, char *password) {
    char sendbuffer[128];
    sprintf_P(sendbuffer,PSTR("AT+CWJAP=\"%s\",\"%s\""),ssid,password);
    return wifi_command(sendbuffer, 20);
}

WIFI_ERROR_MESSAGE_t wifi_command_disable_echo()
{
    return wifi_command("ATE0", 1);
}

WIFI_ERROR_MESSAGE_t wifi_command_with_response(const char *command, char *response, uint8_t maxResponseLength, uint8_t timeOut_s) {
    uint32_t cmdstarttime;
    
    wifi_clear_rxring();
    #ifdef WIFI_DEBUG
       uart_send_string_blocking(USART_0, "WIFITX: ");
       uart_send_string_blocking(USART_0, command);
       uart_send_string_blocking(USART_0, "\n");
    #endif

    uart_send_string_blocking(USART_WIFI, command);
    uart_send_string_blocking(USART_WIFI, "\r\n");
    memset(response,0,maxResponseLength);
    uint8_t responseLength = 0;
    cmdstarttime=k_uptime_get();
    while (k_uptime_get()-cmdstarttime<=timeOut_s) {
        if (k_sem_take(&wifi_rx_sem,K_MSEC(1000))==0) { // received chars
            const uint8_t key = irq_lock();
            char rxchar;
            while (k_ring_pop(&wifi_rx_ring,&rxchar)==0 && responseLength<maxResponseLength) {
                response[responseLength++] = rxchar;
            }
            irq_unlock(key);
            if (strstr((char *)response, "OK\r\n") != NULL
              ||strstr((char *)response, "ERROR\r\n") != NULL
              ||strstr((char *)response, "FAIL\r\n") != NULL) {
                break;
            }
        } 
    }
    #ifdef WIFI_DEBUG
       uart_send_string_blocking(USART_0, "WIFIRX: ");
       uart_send_string_blocking(USART_0, (char *)response);
       uart_send_string_blocking(USART_0, "\n");
    #endif
    WIFI_ERROR_MESSAGE_t error;

    if (responseLength == 0)
        error=WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)response, "OK") != NULL)
        error=WIFI_OK;
    else if (strstr((char *)response, "ERROR") != NULL)
        error= WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)response, "FAIL") != NULL)
        error= WIFI_FAIL;
    else
        error= WIFI_ERROR_RECEIVING_GARBAGE;
    
    return error; 
}

WIFI_ERROR_MESSAGE_t wifi_command_get_ip_from_domain(const char *domain, char *ip_address) {
    char sendbuffer[128];

    strcpy(sendbuffer, "AT+CIPDOMAIN=\"");
    strcat(sendbuffer, domain);
    strcat(sendbuffer, "\"");

    WIFI_ERROR_MESSAGE_t error = wifi_command_with_response(sendbuffer,sendbuffer,127,5); 
    if (error == WIFI_OK) {
        char *ipStart = strstr((char *)sendbuffer, "CIPDOMAIN:");
        if (ipStart != NULL) {
            // Move the pointer to the start of the IP address
            ipStart += strlen("CIPDOMAIN:");

            // Find the end of the IP address (assuming it ends with a newline)
            char * ipEnd = strchr(ipStart, '\r');
            if (ipEnd != NULL && (ipEnd - ipStart) < 16) {
                // Copy the IP address into the buffer
                strncpy(ip_address, ipStart, ipEnd - ipStart);
                ip_address[ipEnd - ipStart] = '\0';
            } 
        }
    }
    return error;
}

WIFI_ERROR_MESSAGE_t wifi_command_quit_AP(){

    return wifi_command("AT+CWQAP", 5);
}

WIFI_ERROR_MESSAGE_t wifi_command_set_mode_to_1()
{
    return wifi_command("AT+CWMODE=1", 1);
}

WIFI_ERROR_MESSAGE_t wifi_command_set_to_single_Connection()
{
    return wifi_command("AT+CIPMUX=0", 1);
}

WIFI_ERROR_MESSAGE_t wifi_command_close_TCP_connection()
{
    return wifi_command("AT+CIPCLOSE", 5);
}



#define IPD_PREFIX "+IPD,"
#define PREFIX_LENGTH 5

int static wifi_TCP_callback(char byte,char *received_message_buffer) {
    static enum { IDLE, MATCH_PREFIX, LENGTH, DATA } state = IDLE;
    static int length = 0, index = 0, prefix_index = 0;
    int hasMessage = 0;

    switch(state) {
        case IDLE:
            if(byte == IPD_PREFIX[0]) {
                state = MATCH_PREFIX;
                prefix_index = 1;
            }
            break;

        case MATCH_PREFIX:
            if(byte == IPD_PREFIX[prefix_index]) {
                if(prefix_index == PREFIX_LENGTH - 1) {
                    state = LENGTH;
                } else {
                    prefix_index++;
                }
            } else {
                // not the expected character, reset to IDLE
                state = IDLE;
                prefix_index = 0;
            }
            break;

        case LENGTH:
            if(byte >= '0' && byte <= '9') {
                length = length * 10 + (byte - '0');
            } else if(byte == ':') {
                state = DATA;
                index = 0; // reset index to start storing data
            } else {
                // not the expected character, reset to IDLE
                state = IDLE;
                length = 0;
            }
            break;

        case DATA:
            if(index < length) {
                received_message_buffer[index++] = byte;
            }
            if(index == length) {
                // message is complete, null terminate the string
                received_message_buffer[index] = 0;

                // reset to IDLE
                state = IDLE;
                length = 0;
                index = 0;

                hasMessage = 1;
            }
            break;
    }
    return hasMessage;
  
}

WIFI_ERROR_MESSAGE_t wifi_command_create_TCP_connection(char *IP, uint16_t port) {
    char sendbuffer[128];
    sprintf_P(sendbuffer,PSTR("AT+CIPSTART=\"TCP\",\"%s\",%u"),IP,port);
    WIFI_ERROR_MESSAGE_t errorMessage = wifi_command(sendbuffer, 20);
    return errorMessage;
}

uint8_t wifi_read_message_from_TCP_connection(char *received_message_buffer,uint8_t timeOut_s) {
    uint32_t cmdstarttime=k_uptime_get();
    char rxchar;
    uint8_t hasMessage=0;

    while (k_uptime_get()-cmdstarttime<=timeOut_s) {
        if (k_sem_take(&wifi_rx_sem,K_MSEC(1000))==0) { // received char
            const uint8_t key = irq_lock();
            while (k_ring_pop(&wifi_rx_ring,&rxchar)==0) {
                hasMessage = wifi_TCP_callback(rxchar,received_message_buffer);
            }
            irq_unlock(key);
            if (hasMessage ==1)
                break;
        } 
    }
    return hasMessage;
}

WIFI_ERROR_MESSAGE_t wifi_command_TCP_transmit(uint8_t * data, uint16_t length){
    char sendbuffer[64];

    sprintf_P(sendbuffer, PSTR("AT+CIPSEND=%u"), length);

    WIFI_ERROR_MESSAGE_t errorMessage = wifi_command(sendbuffer, 20);
    if (errorMessage != WIFI_OK)
        return errorMessage;
    
    uart_send_array_blocking(USART_WIFI, data,  length);
    return WIFI_OK;
}
//This is a ported and fixed version of https://github.com/jandrassy/WiFiEspAT/issues/8#issuecomment-927350290
uint32_t wifi_ntpTime() {
    char sendbuffer[128];
    // Output of AT+CIPSNTPTIME? is +CIPSNTPTIME:Sun Sep 26 13:24:51 2021
    strcpy(sendbuffer, "AT+CIPSNTPTIME?");

    WIFI_ERROR_MESSAGE_t error = wifi_command_with_response(sendbuffer,sendbuffer,127,5); 
    if (error != WIFI_OK || strstr((char *)sendbuffer, "+CIPSNTPTIME") == NULL)
      return 0;
  
    uint32_t y, m, d, hh, mm, ss;		// We use 32 bits for all variables for easier calculations later.
  
    // Name of Day (not used)
    char* tok = strtok(sendbuffer + strlen("+CIPSNTPTIME:"), " ");

  // Month
  tok = strtok(NULL, " ");
  switch( *tok )
  {
	  case 'J':		// Jan, Jun, Jul
		if( *(tok+1) == 'a' ) {		// Jan
			m = 1;
		} else if( *(tok+2) == 'n' ) {	// Jun
			m = 6;
		} else	{					// Jul
			m = 7;
		}
		break;
	  case 'F':		// Feb
		m = 2;
		break;
	  case 'M':		
        if( *(tok+2) == 'r' ) { // Mar
		    m = 3;
        } else { // May
            m = 5;
        }
		break;
	  case 'A':		
		if( *(tok+1) == 'p' ) {		// Apr
			m = 4;
        } else { // Aug
		    m = 8;
        }
		break;
	  case 'S':		// Sep
		m = 9;
		break;
	  case 'O':		// Oct
		m = 10;
		break;
	  case 'N':		// Nov
		m = 11;
		break;
	  case 'D':		// Dec
		m = 12;
		break;
	  default:		// Error
		m = 0;
		break;
  }
  
  // Day
  tok = strtok(NULL, " ");
  d = strtoul(tok, NULL, 10);
  
  // Time
  tok = strtok(NULL, " ");
  hh = 10 * (*(tok+0) - '0') + (*(tok+1) - '0');
  mm = 10 * (*(tok+3) - '0') + (*(tok+4) - '0');
  ss = 10 * (*(tok+6) - '0') + (*(tok+7) - '0');
  
  // Year
  tok = strtok(NULL, " ");
  //yOff = strtoul(tok, NULL, 10) - 2000;
  y = strtoul(tok, NULL, 10);


  // Convert to Epoch
  uint8_t daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  uint32_t yday;
  uint8_t i;  
  
  yday = d;

  for (i = 1; i < m; i++)
    yday += daysInMonth[i-1];

  if(m > 2)
  {
	// Detect leap years
	if( ((y % 4 == 0) && (y % 100!= 0)) || (y%400 == 0) )
    {
      yday++;
	}
  }
  
  y = y - 1900;
  yday = yday - 1;	// The following formula uses "days since January 1 of the year".
  
  // Formula from https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
  unsigned long res = ss + mm*60 + hh*3600 + yday*86400 + (y-70)*31536000 + ((y-69)/4)*86400 - ((y-1)/100)*86400 + ((y+299)/400)*86400;
  return res;    
}