#include <unity.h>
#include <avrtos\avrtos.h>
#include "dht11.h"
#include "light.h"
#include "includes.h"

#ifndef PIO_UNIT_TESTING
#endif

//Clean the project before testing. Sometimes it doesn't recognizes the test cases.

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_dht_read(void){
  unsigned char tempInt;
  unsigned char tempDec;
  unsigned char humInt;
  unsigned char humDec;
  uint8_t dht11_getResult;

  dht11_init();
  dht11_getResult = dht11_get(&humInt, &humDec, &tempInt, &tempDec);
  TEST_ASSERT_EQUAL_UINT8(DHT11_OK,dht11_getResult);
  TEST_ASSERT_UINT8_WITHIN (20, 40, humInt);
  TEST_ASSERT_UINT8_WITHIN (10, 25, tempInt);
}

void test_light_read(void){
  light_init();
  uint16_t lightValue = light_read();
  TEST_ASSERT_UINT16_WITHIN (399, 400, lightValue);
}

int runUnityTests(void) {
  sei();
  z_avrtos_init();
  UNITY_BEGIN();
  RUN_TEST(test_dht_read);
  RUN_TEST(test_light_read);
  return UNITY_END();
}

int main(void) {
  return runUnityTests();
}
void setup() {
  // Wait ~2 seconds before the Unity test runner
  // establishes connection with a board Serial interface
  k_sleep(K_SECONDS(2));

  runUnityTests();
}
void loop() {}
