#include "dht11.h"
#include "includes.h"

//Data (The data goes both ways)
#define DATA_BIT PL1
#define DATA_PIN PINL
#define DATA_DDR DDRL
#define DATA_PORT PORTL

void dht11_init() {
	// set port to output
    DATA_DDR |= (1 << DATA_BIT);

	// pull up
    DATA_PORT |= (1<<DATA_BIT);
}


#define MAX_TIMINGS	85
DHT11_ERROR_MESSAGE_t dht11_get(uint8_t* humidity_integer, uint8_t*  humidity_decimal, uint8_t* temperature_integer, uint8_t* temperature_decimal)
{
    
	uint8_t laststate	= 1;
	uint8_t counter		= 0;
	uint8_t j			= 0, i;
    uint8_t data[5] ;
	data[0] = data[1] = data[2] = data[3] = data[4] = 0;

	// set port to output
    DATA_DDR |= (1 << DATA_BIT);

	/* pull pin down for 18 milliseconds */
    DATA_PORT &= ~(1<<DATA_BIT);
    k_msleep(20);
    
	cli();
	// pull up for 15us
    DATA_PORT |= (1<<DATA_BIT);
    _delay_us(15);

	/* prepare to read the pin */
    DATA_DDR &= ~(1<<DATA_BIT);
    DATA_PORT |= (1<<DATA_BIT); // Do I have to pullup?

	/* detect change and read data */
	for ( i = 0; i < MAX_TIMINGS; i++ )	{
		counter = 0;
		while ( (DATA_PIN&(1<<DATA_BIT)) == laststate )	{
			counter++;
			_delay_us( 1 );
			if ( counter == 255 ) {
				break;
			}
		}
		laststate = (DATA_PIN&(1<<DATA_BIT));
		if ( counter == 255 )
			break;
		/* ignore first 3 transitions */
		if ( (i >= 4) && (i % 2 == 0) )
		{
			/* shove each bit into the storage bytes */
			data[j / 8] <<= 1;
			if ( counter > 36 )
				data[j / 8] |= 1;
			j++;
		}
	}
	sei();

	// have to wait 100us*40 bit= 4ms, then push the bus to idle.
	k_msleep(4);
	// set port to output
    DATA_DDR |= (1 << DATA_BIT);
	// pull up
    DATA_PORT |= (1<<DATA_BIT);

	/*
	 * check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
	 * print it out if data is good
	 */
	if ( (j >= 40) &&
	     (data[4] == ( (data[0] + data[1] + data[2] + data[3]) & 0xFF) ) )
	{
		if (humidity_integer!=NULL)
            *humidity_integer=data[0];
		if (humidity_decimal!=NULL)
            *humidity_decimal=data[1];
        if (temperature_integer!=NULL)
            *temperature_integer = data[2];
        if (temperature_decimal!=NULL)
            *temperature_decimal = data[3];
        return DHT11_OK;
	} else {
        *humidity_integer=*humidity_decimal=*temperature_integer=*temperature_decimal =0;
        return DHT11_FAIL;
	}
}