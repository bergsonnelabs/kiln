#ifndef INC_TILE_SENSE_A_D3LS_H_
#define INC_TILE_SENSE_A_D3LS_H_

#include "main.h"
#include "stdint.h"

#define LIS2DS12_I2C_ADDR				0x1E

#define LIS2DS12_REG_WHO_AM_I			0x0F

#define LIS2DS12_REG_WHO_AM_I_DEFAULT	0x43

/**********************************************************

	FIND THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_a_d3ls_find(I2C_HandleTypeDef* hi2c);

#endif
