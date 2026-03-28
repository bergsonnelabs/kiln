#ifndef INC_TILE_DISP_GRID_H_
#define INC_TILE_DISP_GRID_H_

#include "main.h"
#include "stdint.h"

#define AS1130_I2C_ADDR				0x30

#define	AS1130_REG_PICTURE			0x00
#define	AS1130_REG_DISPLAY_OPTION	0x04
#define AS1130_REG_CURRENT_SOURCE	0x05
#define AS1130_REG_CONFIG			0x06
#define AS1130_REG_SHUTDOWN			0x09

#define AS1130_REG_SELECTION		0xFD
#define AS1130_SEL_DOT_CORRECTION	0x80
#define AS1130_SEL_CONTROL			0xC0

/**********************************************************

	FIND THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_disp_grid_check(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	-

**********************************************************/
void tile_disp_grid_init(I2C_HandleTypeDef* hi2c);

#endif
