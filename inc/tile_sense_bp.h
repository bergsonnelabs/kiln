#ifndef INC_TILE_SENSE_BP_H_
#define INC_TILE_SENSE_BP_H_

#include "main.h"
#include "stdint.h"

#define ILPS22QS_I2C_ADDR				0x5C

#define ILPS22QS_REG_WHO_AM_I			0x0F
#define ILPS22QS_REG_CTRL_REG1			0x10
#define ILPS22QS_REG_PRESS_OUT_XL		0x28
#define ILPS22QS_REG_PRESS_OUT_L		0x29
#define ILPS22QS_REG_PRESS_OUT_H		0x2A

#define ILPS22QS_REG_WHO_AM_I_DEFAULT	0xB4

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_bp_check(I2C_HandleTypeDef* hi2c);

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_bp_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	READ THE 24-BIT PRESSURE VALUE

	parameters
	-------------------------------------------------------
	-

	returns
	-------------------------------------------------------
	success		24-bit right-shifted pressure value

**********************************************************/
uint32_t tile_sense_bp_get_pressure(void);

#endif
