#ifndef INC_TILE_DRIVE_DC_H_H_
#define INC_TILE_DRIVE_DC_H_H_

#include "main.h"
#include "stdint.h"

#define TILE_DRIVE_DC_H_MODE_I2C			0
#define MODE_PADS							1

#define DRV8214_I2C_ADDR					0x34

#define DRV8214_REG_CONFIG0					0x09
#define DRV8214_REG_CONFIG1					0x0A
#define	DRV8214_REG_CONFIG2					0x0B
#define	DRV8214_REG_CONFIG3					0x0C
#define DRV8214_REG_CONFIG4					0x0D
#define DRV8214_REG_CTRL0					0x0E
#define DRV8214_REG_CTRL1					0x0F

#define DRV8214_REG_CONFIG3_DEFAULT			0x63

/**********************************************************

	CHECK IF THE TILE IS PRESENT

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_dc_h_find(I2C_HandleTypeDef* hi2c);

/**********************************************************

	CONFIGURE THE TILE FOR I2C

	parameters
	-------------------------------------------------------
	mode		0 = I2C, 1 = pads

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/

void tile_drive_dc_h_config(uint8_t mode);

void tile_drive_dc_h_output(uint8_t en, uint8_t dir);

#endif
