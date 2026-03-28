#include "tile_sense_a_d3ls.h"

//static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_a_d3ls_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, LIS2DS12_I2C_ADDR<<1, LIS2DS12_REG_WHO_AM_I, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] == LIS2DS12_REG_WHO_AM_I_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}
