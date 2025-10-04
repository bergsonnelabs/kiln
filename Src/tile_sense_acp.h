#ifndef INC_TILE_SENSE_ACP_H_
#define INC_TILE_SENSE_ACP_H_

#include "main.h"
#include "stdint.h"

#define TMD3725_I2C_ADDR				0x39

#define TMD3725_REG_ID					0x92

#define TMD3725_REG_ID_DEFAULT			0xE4


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_acp_check(I2C_HandleTypeDef* hi2c);

#endif
