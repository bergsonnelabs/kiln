#ifndef INC_TILE_SENSE_CAM_H_
#define INC_TILE_SENSE_CAM_H_

#include "main.h"
#include "stdint.h"

#define MIRA016_I2C_ADDR				0x36

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_cam_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_cam_init(I2C_HandleTypeDef* hi2c);

#endif
