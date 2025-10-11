#include "tile_drive_da_ib.h"
#include "main.h"


static I2C_HandleTypeDef* tile_handle;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
uint16_t dac530a2w_read(uint8_t reg){
	uint16_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, DAC530A2W_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 2, 1000);
	return RX_Buffer[0];
}

void dac530a2w_write(uint8_t reg, uint16_t value){
	uint16_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, DAC530A2W_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}


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

uint8_t tile_drive_da_ib_config(I2C_HandleTypeDef* hi2c){
	tile_handle = hi2c;

	// current mode for channel 2
	// DAC-PDN-2 = 00b

	// --------------------------------------------------------
	// COMMON-CONFIG
	// --------------------------------------------------------
	//	15		WIN-LATCH-EN	0		0
	//	14		DEV-LOCK		0		0
	//	13		EE-READ_ADDR	0		0
	//	12		EN-INT-REF		0		0
	//	11:10	DAC-PDN-1		11		11		power down in Hi-Z mode
	//	9		-				1		1
	//	8:7		DAC-PDN-0		11		11		power down inb Hi-Z mode
	//	6:3		-				1111	1111
	//	2:1		DAC-PDN-2		11		00		power up
	//	0		-				1		1
	// --------------------------------------------------------

	dac530a2w_write(DAC530A2W_REG_COMMON_CONFIG, 0b0000111111111001);
	// __builtin_bswap16

	// --------------------------------------------------------
	// DAC-2-GAIN-CONFIG
	// --------------------------------------------------------
	//	15:13	-				000
	//	12:10	IOUT-GAIN		000				000 = 2/3; 001 = 1/2
	//	9:0		-				0:0
	// (just don't touch it for now)
}

void tile_drive_da_ib_output(int16_t value)
{
	dac530a2w_write(DAC530A2W_REG_DAC_2_DATA, value<<4);
}
