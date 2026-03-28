#include "tile_sense_mic.h"

static I2C_HandleTypeDef* tile_handle;


// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_mic_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, MAX11645_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_mic_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

	// SETUP
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	// 	7		REG						1		1 = setup, 0 = configuration
	//	6:4		V_REF			000				000 = Vdd
	//	3		CLK				0				0 = internal, 1 = external
	//	2		BIP/UNI			0				1 = bipolar, 0 = unipolar
	//	1		RST								1 = no action, 0 = reset config register to default
	//	0		-								doesn't matter

	// CONFIGURATION
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	// 	7		REG						0		1 = setup, 0 = configuration
	//	6:5		SCAN			00		00		00 = scan from AIN0 to CS0
	//	4:2		-
	//	1		CS0				0		0		0 = AIN0-GND, 1 = AIN1-GND
	//	0		SGL/DIF			1		1		0 = diff, 1 = single-ended (default)

	return 1;
}

uint16_t tile_sense_mic_get_data(void)
{
	uint16_t RX_Buffer[1];
	HAL_I2C_Master_Receive(tile_handle, MAX11645_I2C_ADDR<<1, (uint8_t*)RX_Buffer, 2, 1000);
	return (0x0FFF & __builtin_bswap16(RX_Buffer[0]));

//	uint8_t RX_Buffer[2];
//	HAL_I2C_Master_Receive(tile_handle, MAX11645_I2C_ADDR, (uint8_t*)RX_Buffer, 2, 1000);
//	return (((uint16_t)RX_Buffer[0] & 0x0F) << 8) + (uint16_t)RX_Buffer[1];

}
