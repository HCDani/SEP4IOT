#include <unity.h>
#include "convert.h"
#include <string.h>


char hexInput[] = "48656c6c6f"; // "Hello" in hex
uint8_t binaryOutput[5];
uint8_t binaryInput[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };
char hexOutput[11];

void setUp(void) {
    
}

void tearDown(void) {
    
}

void test_fromHex(void) {
    fromHex(hexInput, binaryOutput);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(binaryInput, binaryOutput, 5);
}

void test_toHex(void) {
    toHex(binaryInput, hexOutput, 5);
    hexOutput[10] = '\0'; // Null-terminate the string
    TEST_ASSERT_EQUAL_STRING(hexInput, hexOutput);
}

void test_calcCRC16(void) {
    uint16_t expectedCRC = 0xF377; //  expected value
    uint16_t crc = calcCRC16(binaryInput, 0, 5);
    TEST_ASSERT_EQUAL_UINT16(expectedCRC, crc);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_fromHex);
    RUN_TEST(test_toHex);
    RUN_TEST(test_calcCRC16);
    return UNITY_END();
}
