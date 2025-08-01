#ifndef INC_TILE_DRIVE_H_H_
#define INC_TILE_DRIVE_H_H_

#include "main.h"
#include "stdint.h"

#define TILE_DRIVE_H_I2C_ADDR				0x5A

#define TILE_DRIVE_H_REG_STATUS				0x00
#define TILE_DRIVE_H_REG_MODE				0x01
#define TILE_DRIVE_H_REG_LIBRARY_SEL		0x03
#define TILE_DRIVE_H_REG_WAVE_SEQ_0			0x04
#define TILE_DRIVE_H_REG_GO					0x0C
#define	TILE_DRIVE_H_REG_FEEDBACK_CTRL		0x1A
#define TILE_DRIVE_H_REG_CONTROL3			0x1D

#define TILE_DRIVE_H_REG_STATUS_DEFAULT		0x60

/**********************************************************

	INITIALIZE THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_drive_h_init(I2C_HandleTypeDef* hi2c);

/**********************************************************

	PLAY A STORED HAPTIC PATTERN

	parameters
	-------------------------------------------------------
	index		the library index (see datasheet)
	repeats		number of times to play the pattern (with a 200ms gap)

	returns
	-------------------------------------------------------
	n/a

**********************************************************/

void tile_drive_h_play(uint8_t index, uint8_t repeats);

#endif
