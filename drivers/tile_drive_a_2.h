#ifndef INC_TILE_DRIVE_A_2_H_
#define INC_TILE_DRIVE_A_2_H_

#include "main.h"
#include "stdint.h"

#define DAC63202W_I2C_ADDR				0x49

#define DAC63202W_REG_DAC_0_DATA		0x1C
#define DAC63202W_REG_DAC_1_DATA		0x19
#define DAC63202W_REG_COMMON_CONFIG		0x1F
#define DAC63202W_REG_GENERAL_STATUS	0x22


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_a_2_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_a_2_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	SET THE DAC OUTPUT

	parameters
	-------------------------------------------------------
	channel		0 or 1
	value		12-bit unsigned

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
void tile_drive_a_2_output(uint8_t channel, uint16_t value);

#endif
