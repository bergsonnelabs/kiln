#include "tile_sense_tof.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
uint8_t tmf8806_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, TMF8806_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void tmf8806_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, TMF8806_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_tof_check(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, TMF8806_I2C_ADDR<<1, TMF8806_REG_ID, 1, (uint8_t *)RX_Buffer, 1, 1000);
	if( (RX_Buffer[0] & 0b00111111) == TMF8806_REG_ID_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_tof_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

//	HAL_Delay(3);
//	tmf8806_write(TMD3725_REG_ENABLE, 0x01); // set PON
//	HAL_Delay(2);
//	tmf8806_write(TMF8806_REG_APPREQID, 0xC0); // start ROM application
//	HAL_Delay(2);
	return 1;
}
