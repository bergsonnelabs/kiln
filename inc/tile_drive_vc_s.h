#ifndef INC_TILE_DRIVE_VC_S_H_
#define INC_TILE_DRIVE_VC_S_H_

#include "main.h"
#include "stdint.h"

#define DRV201_I2C_ADDR						0x0E

#define DRV201_REG_CONTROL					0x02

#define DRV201_REG_CONTROL_DEFAULT			0x02


/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_vc_s_find(I2C_HandleTypeDef* hi2c);

#endif
