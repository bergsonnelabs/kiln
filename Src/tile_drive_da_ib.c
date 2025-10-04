#include "tile_drive_da_ib.h"
#include "main.h"


static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
/*uint8_t ilps22qs_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, ILPS22QS_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void ilps22qs_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, ILPS22QS_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}
*/

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_drive_da_ib_check(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[2];
	HAL_I2C_Mem_Read(hi2c, DAC530A2W_I2C_ADDR<<1, DAC530A2W_REG_GEN_STATUS, 1, (uint8_t *)RX_Buffer, 2, 1000);

	if(RX_Buffer[1] == DAC530A2W_REG_GEN_STATUS_L_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}
