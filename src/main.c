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

#define low(x)   ((x) & 0xFF)
#define high(x)   (((x)>>8) & 0xFF)

//This idea came from the following source: https://forum.arduino.cc/t/solved-split-uint-32-to-bytes/532751/9
typedef union {
  uint32_t v;
  uint8_t as_bytes[4];
} uint32_split_t, *uint32_split_p;

void thread_sensor(void *p);
void thread_serial(void *p);
void thread_actuators(void *p);
K_THREAD_DEFINE(sensor, thread_sensor, 0x100, K_PRIO_DEFAULT, NULL, 'R');
K_THREAD_DEFINE(serial, thread_serial, 0x100, K_PRIO_DEFAULT, NULL, 'S');
K_THREAD_DEFINE(actuators, thread_actuators, 0x100, K_PRIO_DEFAULT, NULL, 'A');
K_MUTEX_DEFINE(globalVariableMutex);
void uart_0_callback(uint8_t receiver){};

unsigned char tempInt;
unsigned char tempDec;
unsigned char humInt;
unsigned char humDec;
uint16_t lightValue;

uint8_t ledState;
uint8_t servoAngle;

const char hex[] PROGMEM = "0123456789abcdef";
static const uint8_t key[] = {0x44,0xde,0xc5,0xcc,0xbd,0xf9,0xc2,0xec,0x53,0xbf,0xd3,0x87,0xdf,0x9f,0x47,0xef};
static uint8_t netData[16];

char netBuffer[60];
char netResponseBuffer[500];

int main(){
  uart_init(USART_0,250000,uart_0_callback);
  wifi_init();
  sei();
  z_avrtos_init();
  wifi_command_disable_echo();
  wifi_command_join_AP("sj17c","sj17c_password");
  k_sleep(K_SECONDS(2));
  wifi_command("AT+CIPSNTPCFG=1,0,\"pool.ntp.org\"", 2);
  uint32_t wifiTime=0;
  for(uint8_t i=0;i<10&&wifiTime==0;i++){
    k_sleep(K_SECONDS(1));
    wifiTime=wifi_ntpTime();
  }
  sprintf(netResponseBuffer,"wifiTime: %lu\n",wifiTime);
  uart_send_string_blocking(USART_0,netResponseBuffer);
  k_time_set(wifiTime);
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
    uint16_t crcval = 0xffff;
    for (uint8_t i = 0; i<14; i++) crcval = _crc16_update(crcval,netData[i]);
    netData[14]=high(crcval);
    netData[15]=low(crcval);
    aes128_enc_single(key, netData);
    wifi_command_create_TCP_connection("mserver.nmprog.hu",80);
    strcpy_P(netBuffer,PSTR("GET /iot/"));
    char *nbout = netBuffer+strlen(netBuffer);
    for (uint8_t i = 0; i<16; i++ , nbout+=2) {
      nbout[0]=pgm_read_byte(&(hex[(netData[i]>>4) & 0xF]));
      nbout[1]=pgm_read_byte(&(hex[netData[i] & 0xF]));
    }
    nbout[0]=0;
    strcat_P(netBuffer,PSTR(" HTTP/1.0\r\n\r\n"));
    wifi_command_TCP_transmit((uint8_t *)netBuffer,strlen(netBuffer));
    wifi_read_message_from_TCP_connection(netResponseBuffer,5);
    wifi_command_close_TCP_connection();
    uart_send_string_blocking(USART_0, netResponseBuffer);
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
  light_init();
  dht11_init();
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
    if(lightValue < 600){
      ledState = 0;
    }else if (lightValue > 700){
      ledState = 1;
    }
#endif
    sprintf(charBuffer,"Temp: %d.%d Hum: %d.%d Light: %d SA: %d LS: %d\n", tempInt, tempDec, humInt, humDec, lightValue, servoAngle, ledState);
    k_mutex_unlock(&globalVariableMutex);
    uart_send_string_blocking(USART_0, charBuffer);
    k_sleep(K_SECONDS(10));
  }
}
void thread_actuators(void *p){
  uint8_t LledState;
  uint8_t LservoAngle;
  leds_init();
  while(1){
    k_mutex_lock(&globalVariableMutex, K_FOREVER);
    LledState = ledState;
    LservoAngle = servoAngle;
    k_mutex_unlock(&globalVariableMutex);
    if(LledState == 0){
      leds_turnOff(1);
    } else{
      leds_turnOn(1);
    }
    servo(LservoAngle);
    k_sleep(K_SECONDS(3));
  }
}