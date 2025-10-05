#include "tile_drive_vc_s.h"
#include "main.h"


static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_drive_vc_s_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, DRV201_I2C_ADDR<<1, DRV201_REG_CONTROL, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] == DRV201_REG_CONTROL_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}
