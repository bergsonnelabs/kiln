#include "tile_sense_a_d3lb.h"

//static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_a_d3lb_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];

	// dummy transaction required to wake things up
	HAL_I2C_Mem_Read(hi2c, BMA530_I2C_ADDR<<1, BMA530_REG_CHIP_ID, 1, (uint8_t *)RX_Buffer, 1, 1000);

	HAL_I2C_Mem_Read(hi2c, BMA530_I2C_ADDR<<1, BMA530_REG_CHIP_ID, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] == BMA530_REG_CHIP_ID_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}
