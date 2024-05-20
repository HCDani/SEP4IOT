
#ifndef _CONVERT_H
#define _CONVERT_H

#include <stdint.h>
#include <avr/pgmspace.h>
#include <util/crc16.h>

void fromHex(char *input, uint8_t *output);
void toHex(uint8_t *input, char *output, uint8_t length);
uint16_t calcCRC16(const uint8_t *input, uint8_t start, uint8_t length);

#endif
