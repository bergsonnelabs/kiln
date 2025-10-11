#include "tile_sense_m_3m.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_m_3m_find(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, MLX90395_I2C_ADDR<<1, MLX90395_REG_CONFIG0, 1, (uint8_t *)RX_Buffer, 2, 1000);

	if(RX_Buffer[0] == MLX90395_REG_CONFIG0_L_DEFAULT){
		return 1;
	} else {
		return 0;
	}
	return 0;
}
