#ifndef INC_TILE_DRIVE_DC_I_H_
#define INC_TILE_DRIVE_DC_I_H_

#include "main.h"
#include "stdint.h"

#define DRV8214_I2C_ADDR					0x34

#define DRV8214_REG_CONFIG0					0x09

#define DRV8214_REG_CONFIG0_DEFAULT			0x60

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_dc_i_find(I2C_HandleTypeDef* hi2c);

#endif
