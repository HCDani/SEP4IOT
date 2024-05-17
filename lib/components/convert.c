#include "convert.h"

const char hex[] PROGMEM = "0123456789abcdef";

void fromHex(char *input, uint8_t *output) {
    char *c;
    while ( c[0]!=0 ) {
        output[0] = (((c[0] & 0xF) + ((c[0] >> 6) | ((c[0] >> 3) & 0x8)))<<4)
                    |((c[1] & 0xF) + ((c[1] >> 6) | ((c[1] >> 3) & 0x8)));
        c+=2;
        output+=1;
    }
}
void toHex(uint8_t *input, char *output, uint8_t length) {
    char *c=output;
    for (uint8_t i = 0; i<length; i++ , c+=2) {
        c[0]=pgm_read_byte(&(hex[(input[i]>>4) & 0xF]));
        c[1]=pgm_read_byte(&(hex[input[i] & 0xF]));
    }
}

uint16_t calcCRC16(const uint8_t *input, uint8_t start, uint8_t length) {
    uint16_t crcval = 0xffff;
    for (uint8_t i = start; i<length; i++) crcval = _crc16_update(crcval,input[i]);
}
