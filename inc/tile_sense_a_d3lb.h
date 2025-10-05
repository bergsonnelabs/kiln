#ifndef INC_TILE_SENSE_A_D3LB_H_
#define INC_TILE_SENSE_A_D3LB_H_

#include "main.h"
#include "stdint.h"

#define BMA530_I2C_ADDR					0x18

#define BMA530_REG_CHIP_ID				0x00

#define BMA530_REG_CHIP_ID_DEFAULT		0xC2

/**********************************************************

	FIND THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_a_d3lb_find(I2C_HandleTypeDef* hi2c);

#endif
