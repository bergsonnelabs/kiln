#include "tile_drive_dc_i.h"
#include "main.h"


static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_drive_dc_i_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, DRV8214_I2C_ADDR<<1, DRV8214_REG_CONFIG0, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] == DRV8214_REG_CONFIG0_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}
