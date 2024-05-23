#include <avrtos\avrtos.h>
#include "dht11.h"
#include "light.h"
#include "includes.h"
#include "uart.h"
#include "leds.h"
#include "servo.h"
#include "wifi.h"
#include <util\delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <AESLib.h>
#include <util/crc16.h>
#include "convert.h"

#define low(x)   ((x) & 0xFF)
#define high(x)   (((x)>>8) & 0xFF)

//This idea came from the following source: https://forum.arduino.cc/t/solved-split-uint-32-to-bytes/532751/9
typedef union {
  uint32_t v;
  uint8_t as_bytes[4];
} uint32_split_t, *uint32_split_p;

void thread_sensor(void *p);
void thread_serial(void *p);
K_THREAD_DEFINE(sensor, thread_sensor, 0x100, K_PRIO_DEFAULT, NULL, 'R');
K_THREAD_DEFINE(serial, thread_serial, 0x100, K_PRIO_DEFAULT, NULL, 'S');
K_MUTEX_DEFINE(globalVariableMutex);
void uart_0_callback(uint8_t receiver){};

volatile unsigned char tempInt;
volatile unsigned char tempDec;
volatile unsigned char humInt;
volatile unsigned char humDec;
volatile uint16_t lightValue;

volatile uint8_t ledState;
volatile uint8_t servoAngle;

static const uint8_t key[] = {0x44,0xde,0xc5,0xcc,0xbd,0xf9,0xc2,0xec,0x53,0xbf,0xd3,0x87,0xdf,0x9f,0x47,0xef};
static uint8_t netData[16];

char netBuffer[120];
char netResponseBuffer[500];

int main(){
  uart_init(USART_0,250000,uart_0_callback);
  wifi_init();
  light_init();
  dht11_init();
  leds_init();

  sei();
  z_avrtos_init();
  wifi_command_disable_echo();
//  wifi_command_join_AP("sj17c","sj17c_password");
  k_sleep(K_SECONDS(2));
  k_time_set(wifi_ntpTime());
  while (1){
    //Board Id
    netData[0]=0;
    netData[1]=1;
    uint32_split_t currTime;
    currTime.v=k_time_get();
    netData[2]=currTime.as_bytes[3];
    netData[3]=currTime.as_bytes[2];
    netData[4]=currTime.as_bytes[1];
    netData[5]=currTime.as_bytes[0];
    k_mutex_lock(&globalVariableMutex, K_FOREVER);
    netData[6]=humInt;
    netData[7]=humDec;
    netData[8]=tempInt;
    netData[9]=tempDec;
    netData[10]=high(lightValue);
    netData[11]=low(lightValue);
    k_mutex_unlock(&globalVariableMutex);
    netData[12]=0;
    netData[13]=0;
    uint16_t crcval = calcCRC16(netData, 0, 14);
    netData[14]=high(crcval);
    netData[15]=low(crcval);

    memset(netBuffer,0,120);
    toHex(netData,netBuffer,16);
    uart_send_string_blocking(USART_0, netBuffer);
    uart_send_blocking(USART_0,'\n');
    aes128_enc_single(key, netData);

    memset(netBuffer,0,120);
    strcpy_P(netBuffer,PSTR("GET /api/Board/"));
    toHex(netData,netBuffer+strlen(netBuffer),16);
    strcat_P(netBuffer,PSTR(" HTTP/1.0\r\nHost: sep4backendapp.azurewebsites.net\r\n\r\n"));
    uart_send_string_blocking(USART_0, netBuffer);
    wifi_http_get("sep4backendapp.azurewebsites.net",80,netBuffer,netResponseBuffer,500);
    char * bodyStart =  strstr(netResponseBuffer, "\r\n\r\n");
    if (bodyStart == NULL || strlen(bodyStart)<36 ) {
      uart_send_string_blocking(USART_0, netResponseBuffer);
    } else {
      strncpy(netBuffer,bodyStart+4,32);
      netBuffer[32]=0;
      uart_send_string_blocking(USART_0, netBuffer);
      uart_send_blocking(USART_0,'\n');

      fromHex(netBuffer,netData);
      aes128_dec_single(key, netData);
      uint16_t server_crcval = calcCRC16(netData, 0, 14);
      uint32_split_t serverTime;
      serverTime.as_bytes[3]=netData[2];
      serverTime.as_bytes[2]=netData[3];
      serverTime.as_bytes[1]=netData[4];
      serverTime.as_bytes[0]=netData[5];
      if (netData[14]==high(server_crcval) && netData[15]==low(server_crcval) // has valid crc
        && netData[0]==0x80 && netData[1]==0x01 // has valid boardid and direction
        && serverTime.v+10>currTime.v ) { // server time have to bigger then our time-10 seconds
        // considering the message valid
        if (netData[6]==1) { // change actuators command
          k_mutex_lock(&globalVariableMutex, K_FOREVER);
          ledState = netData[7];
          servoAngle = netData[8];
          k_mutex_unlock(&globalVariableMutex);
            // led,servo
          if(netData[7] == 0){
            leds_turnOff(1);
          } else{
            leds_turnOn(1);
          }
          servo(netData[8]==1?180:0);
        }
      }

      memset(netBuffer,0,120);
      toHex(netData,netBuffer,16);
      uart_send_string_blocking(USART_0, netBuffer);
      uart_send_blocking(USART_0,'\n');
    }
    k_sleep(K_SECONDS(11));
  }
  return 0;
}

void thread_sensor(void *p){
unsigned char LtempInt;
unsigned char LtempDec;
unsigned char LhumInt;
unsigned char LhumDec;
uint16_t LlightValue;
uint8_t Ldht11_getResult;
	while (1) {
    LlightValue = light_read();
    Ldht11_getResult = dht11_get(&LhumInt, &LhumDec, &LtempInt, &LtempDec);
    k_mutex_lock(&globalVariableMutex, K_FOREVER);
    lightValue = LlightValue;
    if(Ldht11_getResult == DHT11_OK){
      humInt = LhumInt;
      humDec = LhumDec;
      tempInt = LtempInt;
      tempDec = LtempDec;
    } else {
      uart_send_blocking(USART_0,'|');
    }
    k_mutex_unlock(&globalVariableMutex);
		k_sleep(K_SECONDS(3));
	}
}

void thread_serial(void *p){
  char charBuffer[40];
  k_sleep(K_SECONDS(10));
  while(1){
    k_mutex_lock(&globalVariableMutex, K_FOREVER);
    
#ifdef ACTUATOR_DEMO
    if(tempInt >= 28){
      servoAngle = 90;
    } else if (tempInt <= 27){
      servoAngle = 0;
    }
    if(lightValue > 400){
      ledState = 0;
    }else if (lightValue < 300){
      ledState = 1;
    }
#endif
    sprintf(charBuffer,"Temp: %d.%d Hum: %d.%d Light: %d SA: %d LS: %d\n", tempInt, tempDec, humInt, humDec, lightValue, servoAngle, ledState);
    k_mutex_unlock(&globalVariableMutex);
    uart_send_string_blocking(USART_0, charBuffer);
    k_sleep(K_SECONDS(10));
  }
}