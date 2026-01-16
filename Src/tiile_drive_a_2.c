#include "tile_drive_a_2.h"

static I2C_HandleTypeDef* tile_handle;


// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_drive_a_2_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, DAC63202W_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}
uint8_t tile_drive_a_2_init(I2C_HandleTypeDef* hi2c)
{

	tile_handle = hi2c;

	uint8_t TX_Buffer[2];

	// COMMON CONFIG (0xE3)
	// -----------------------------------------------------------
	//	BITS					DEF		SET		NOTES
	// -----------------------------------------------------------
	// 	15		WIN-LATCH-EN	0		0		window comparator (0 = non-latching; 1 = latching)
	//	14		DEV-LOCK		0		0		device lock (0 = not locked; 1 = lock all registers)
	//	13		EE-READ-ADDR	0		0		fault-dump read enable at address (0 = 0x00, 1 = 0x01)
	//	12		EN-INT-REF		0		0		internal reference enable (0=disable; 1=enable)
	//	11:10	VOUT-PDN-0		11		00		VOUT-0 enable (00 = power up; 01 = 1-k to AGND; 10 = 100k to AGND; 11 = Hi-Z)
	//	9		IOUT-PDN-0		1		1		IOUT-0 enable (0 = power up; 1 = power down)
	//	2:1		VOUT-PDN-1		11		00		VOUT-1 enable (00 = power up; 01 = 1-k to AGND; 10 = 100k to AGND; 11 = Hi-Z)
	//	0		IOUT-PDN-1		1		1		IOUT-1 enable (0 = power up; 1 = power down)

	// puwer up VOUT-X and power down IOUT-X
	TX_Buffer[0] = 0x02;
	TX_Buffer[1] = 0x01;

	HAL_I2C_Mem_Write(tile_handle, DAC63202W_I2C_ADDR<<1, DAC63202W_REG_COMMON_CONFIG, 1, (uint8_t *)TX_Buffer, 2, 1000);

	// DAC-X-VOUT-CMP-CONFIG (0x15, 0x03)
	// -----------------------------------------------------------
	// default gain = 1x reference to VREF (which is low-pass filtered from VDD), so nothing to change

	return 1;
}

void tile_drive_a_2_output(uint8_t channel, uint16_t value)
{

	uint16_t TX_Buffer[1] = {value << 4};
	if(channel){ // channel 1
		HAL_I2C_Mem_Write(tile_handle, DAC63202W_I2C_ADDR<<1, DAC63202W_REG_DAC_1_DATA, 1, (uint8_t *)TX_Buffer[0], 2, 1000);
	} else { // channel 0
		HAL_I2C_Mem_Write(tile_handle, DAC63202W_I2C_ADDR<<1, DAC63202W_REG_DAC_0_DATA, 1, (uint8_t *)TX_Buffer[0], 2, 1000);
	}

	// 12 bit, left aligned


	/*
program the DAC code in the DAC-X-DATA register of the respective channels
*/

}
