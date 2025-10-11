#include "tile_sense_co2.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_co2_find(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
//	uint8_t TX_Buffer[1] = {STC31_CMD_ID1B};
	HAL_I2C_Mem_Write(hi2c, STC31_I2C_ADDR<<1, STC31_CMD_ID1A, 1, STC31_CMD_ID1B, 1, 1000);
	HAL_I2C_Mem_Write(hi2c, STC31_I2C_ADDR<<1, STC31_CMD_ID2A, 1, STC31_CMD_ID2B, 1, 1000);


	HAL_I2C_Mem_Read(hi2c, STC31_I2C_ADDR<<1, STC31_REG_ID, 2, (uint8_t *)RX_Buffer, 1, 1000);
if(1){
//	if(RX_Buffer[0] == TMD3725_REG_ID_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}
