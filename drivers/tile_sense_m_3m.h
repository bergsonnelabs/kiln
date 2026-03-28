#ifndef INC_TILE_SENSE_M_3M_H_
#define INC_TILE_SENSE_M_3M_H_

#include "main.h"
#include "stdint.h"

// A1 is pulled high in the Sketching socket board
#define MLX90395_I2C_ADDR				0x0E // A0 floating (low) & A1 high
//#define MLX90395_I2C_ADDR				0x09 // A0 & A1 low (maybe interall pull downs?)

#define MLX90395_REG_CONFIG0			0x00
//  -----------------------------------------------------------
// 	BIT(S)	NAME			DEFAULT
//	-----------------------------------------------------------
//	7:4		GainSel			1000/1001 (high-field) or 1001 (low field)
//	3:0		HallConf		0000
//	15		Lock_HS			0
//	14		Lock_WR			0
//	13		<res>			0
//	12:11	TrimDelSDAOut	0
//	10:9	TrimDelSDAIn	1
//	8		ZSeries			0
//	-----------------------------------------------------------

#define MLX90395_REG_CONFIG1			0x01



/**********************************************************

	FIND THE TILE

	parameters
	-------------------------------------------------------
	hi2c		handle to the pre-configured I2C port

	returns
	-------------------------------------------------------
	success		1 = success, 0 = error

**********************************************************/
uint8_t tile_sense_m_3m_find(I2C_HandleTypeDef* hi2c);

uint8_t tile_sense_m_3m_init(I2C_HandleTypeDef* hi2c);

void tile_sense_m_3m_get_xyzt(int16_t* buffer);

#endif
