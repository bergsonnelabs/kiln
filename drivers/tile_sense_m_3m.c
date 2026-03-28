#include "tile_sense_m_3m.h"

static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------
uint8_t tile_sense_m_3m_find(I2C_HandleTypeDef* hi2c)
{

	HAL_Delay(10);
	if(HAL_I2C_IsDeviceReady(hi2c, MLX90395_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_m_3m_init(I2C_HandleTypeDef* hi2c){
	uint8_t RX_Buffer[1];
	uint8_t TX_Buffer[1];

	tile_handle = hi2c;

	// warm reset (RT)
	TX_Buffer[0] = 0xF0;
	HAL_I2C_Mem_Write(hi2c, MLX90395_I2C_ADDR<<1, 0x80, 1, (uint8_t *)TX_Buffer, 1, 1000);

	HAL_Delay(50);

	// start burst (SM)
	HAL_I2C_Mem_Read(hi2c, MLX90395_I2C_ADDR<<1, 0x8016, 2, RX_Buffer, 1, 1000);
	// response is the STATUS byte
	// 7:4  mode (1000 = burst)
	// 3 	CE/DED flag
	// 2	OVF flag
	// 1	RST flag
	// 0	DRDY flag
	// for the start of burst, I'd expect to see 1xxx0001

	return RX_Buffer[0]; // 10001010 .. why is CE/DED set, and no measurements?
}

void tile_sense_m_3m_get_xyzt(int16_t* buffer){
	HAL_I2C_Mem_Read(tile_handle, MLX90395_I2C_ADDR<<1, 0x80, 1, (uint8_t *)buffer, 12, 1000);
	// status - CRC - XH - XL - YH - YL - ZH - ZL - TH - TL - VH - VL
}
