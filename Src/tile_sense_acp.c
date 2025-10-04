#include "tile_sense_acp.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
uint8_t tmd3725_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, TMD3725_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void tmd3725_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, TMD3725_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_acp_check(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, TMD3725_I2C_ADDR<<1, TMD3725_REG_ID, 1, (uint8_t *)RX_Buffer, 1, 1000);
	if(RX_Buffer[0] == TMD3725_REG_ID_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_acp_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

	// validate connectivity by reading the contents of a known register
	if(tmd3725_read(TMD3725_REG_ID) != TMD3725_REG_ID_DEFAULT){
		return 0;
	}
	tmd3725_write(TMD3725_REG_ENABLE, 0x08); // activate everything
	return 1;
}

uint8_t tile_sense_acp_get_pdata()
{
	return tmd3725_read(TMD3725_REG_PDATA);
}
