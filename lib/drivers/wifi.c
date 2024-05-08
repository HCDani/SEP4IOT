#include "wifi.h"
#include "includes.h"
#include "uart.h"
#include <avrtos\avrtos.h>

#define WIFI_DATABUFFERSIZE 128
#define WIFI_RX_RINGSIZE 16
K_RING_DEFINE(wifi_rx_ring,WIFI_RX_RINGSIZE);
K_SEM_DEFINE(wifi_rx_sem,0,1);

static uint8_t wifi_dataBuffer[WIFI_DATABUFFERSIZE];
static uint8_t wifi_dataBufferIndex;

void static wifi_serial_callback(uint8_t received_byte);


void wifi_init() {
    uart_init(USART_WIFI, 115200, wifi_serial_callback);
}

void static wifi_clear_rxbuffers() {
    const uint8_t key = irq_lock();
    k_ring_init(&wifi_rx_ring,wifi_rx_ring_buf,WIFI_RX_RINGSIZE);
//    while (k_ring_pop(&wifi_rx_ring,(char *)&wifi_dataBufferIndex)==0);
    irq_unlock(key);
    k_sem_take(&wifi_rx_sem,K_NO_WAIT);
    memset(wifi_dataBuffer,0,WIFI_DATABUFFERSIZE);
    wifi_dataBufferIndex = 0;
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

WIFI_ERROR_MESSAGE_t wifi_command(const char *str, uint16_t timeOut_s) {
    char rxchar;
    uint32_t cmdstarttime;

    wifi_clear_rxbuffers();
    #ifdef WIFI_DEBUG
       uart_send_string_blocking(USART_0, "WIFITX: ");
       uart_send_string_blocking(USART_0, (char *)str);
       uart_send_string_blocking(USART_0, "\n");
    #endif
    uart_send_string_blocking(USART_WIFI, str);
    uart_send_string_blocking(USART_WIFI, "\r\n");

    cmdstarttime=k_uptime_get();

    while (k_uptime_get()-cmdstarttime<=timeOut_s) {
        if (k_sem_take(&wifi_rx_sem,K_MSEC(1000))==0) { // received char
            const uint8_t key = irq_lock();
            while (k_ring_pop(&wifi_rx_ring,&rxchar)==0 && wifi_dataBufferIndex<WIFI_DATABUFFERSIZE) {
                wifi_dataBuffer[wifi_dataBufferIndex] = rxchar;
                wifi_dataBufferIndex++;
            }
            irq_unlock(key);
            if (strstr((char *)wifi_dataBuffer, "OK\r\n") != NULL 
                ||wifi_dataBufferIndex==WIFI_DATABUFFERSIZE)
                break;
        } 
    }

    WIFI_ERROR_MESSAGE_t error;

    if (wifi_dataBufferIndex == 0)
        error=WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)wifi_dataBuffer, "OK") != NULL)
        error=WIFI_OK;
    else if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL)
        error= WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL)
        error= WIFI_FAIL;
    else
        error= WIFI_ERROR_RECEIVING_GARBAGE;
    
    #ifdef WIFI_DEBUG
       uart_send_string_blocking(USART_0, "WIFIRX: ");
       uart_send_string_blocking(USART_0, (char *)wifi_dataBuffer);
       uart_send_string_blocking(USART_0, "\n");
    #endif
    return error; 
}

WIFI_ERROR_MESSAGE_t wifi_command_AT()
{
    return wifi_command("AT", 10);
}

WIFI_ERROR_MESSAGE_t wifi_command_join_AP(char *ssid, char *password)
{
   /* WIFI_ERROR_MESSAGE_t error = wifi_command_AT();
    if (error != WIFI_OK)
        return error;*/

    char sendbuffer[128];
    strcpy(sendbuffer, "AT+CWJAP=\"");
    strcat(sendbuffer, ssid);
    strcat(sendbuffer, "\",\"");
    strcat(sendbuffer, password);
    strcat(sendbuffer, "\"");

    return wifi_command(sendbuffer, 20);
}

WIFI_ERROR_MESSAGE_t wifi_command_disable_echo()
{
    return wifi_command("ATE0", 1);
}

WIFI_ERROR_MESSAGE_t wifi_command_get_ip_from_URL(char * url, char *ip_address){
    uint32_t cmdstarttime;
    char sendbuffer[128];
    char rxchar;
    
    strcpy(sendbuffer, "AT+CIPDOMAIN=\"");
    strcat(sendbuffer, url);
    strcat(sendbuffer, "\"");
    
    uint16_t timeOut_s = 5;

    wifi_clear_rxbuffers();

    uart_send_string_blocking(USART_WIFI, strcat(sendbuffer, "\r\n"));

    cmdstarttime=k_uptime_get();

    while (k_uptime_get()-cmdstarttime<=timeOut_s) {
        if (k_sem_take(&wifi_rx_sem,K_MSEC(1000))==0) { // received char
            const uint8_t key = irq_lock();
            while (k_ring_pop(&wifi_rx_ring,&rxchar)==0) {
                wifi_dataBuffer[wifi_dataBufferIndex] = rxchar;
                wifi_dataBufferIndex++;
            }
            irq_unlock(key);
            if (strstr((char *)wifi_dataBuffer, "OK\r\n") != NULL)
                break;
        } 
    }

    WIFI_ERROR_MESSAGE_t error;

    if (wifi_dataBufferIndex == 0)
        error=WIFI_ERROR_NOT_RECEIVING;
    else if (strstr((char *)wifi_dataBuffer, "OK") != NULL)
        error=WIFI_OK;
    else if (strstr((char *)wifi_dataBuffer, "ERROR") != NULL)
        error= WIFI_ERROR_RECEIVED_ERROR;
    else if (strstr((char *)wifi_dataBuffer, "FAIL") != NULL)
        error= WIFI_FAIL;
    else
        error= WIFI_ERROR_RECEIVING_GARBAGE;
    

    char *ipStart = strstr((char *)wifi_dataBuffer, "CIPDOMAIN:");
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
    char portString[7];

    strcpy(sendbuffer, "AT+CIPSTART=\"TCP\",\"");
    
    strcat(sendbuffer, IP);
    strcat(sendbuffer, "\",");
    sprintf(portString, "%u", port);
    strcat(sendbuffer, portString);

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
    char sendbuffer[128];

    sprintf(sendbuffer, "AT+CIPSEND=%u", length);

    WIFI_ERROR_MESSAGE_t errorMessage = wifi_command(sendbuffer, 20);
    if (errorMessage != WIFI_OK)
        return errorMessage;
    
    uart_send_array_blocking(USART_WIFI, data,  length);
    return WIFI_OK;
}