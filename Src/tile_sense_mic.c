#include "tile_sense_mic.h"

static I2C_HandleTypeDef* tile_handle;


// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_mic_check(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[2];
	if(HAL_I2C_Master_Receive(hi2c, MAX11645_I2C_ADDR, RX_Buffer, 2, 1000) == 0){ // returns HAL_StatusTypeDef; 0x00 = OK, else issues
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_mic_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;
	return 1;
}

uint16_t tile_sense_mic_get_data(void)
{
	uint8_t RX_Buffer[2];
	HAL_I2C_Master_Receive(tile_handle, MAX11645_I2C_ADDR, RX_Buffer, 2, 1000);
	return (uint16_t) RX_Buffer[0];
}
