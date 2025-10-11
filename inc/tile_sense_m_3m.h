#ifndef INC_TILE_SENSE_M_3M_H_
#define INC_TILE_SENSE_M_3M_H_

#include "main.h"
#include "stdint.h"

#define MLX90395_I2C_ADDR				0x0E // with A1 (pin 8) pulled high

#define MLX90395_REG_CONFIG0			0x00
//  -----------------------------------------------------------
// 	BIT(S)	NAME			DEFAULT
//	-----------------------------------------------------------
//	7:4		GainSel			1000 (high-field) or 1001 (low field)
//	3:0		HallConf		0000
//	15		Lock_HS			0
//	14		Lock_WR			0
//	13		<res>			0
//	12:11	TrimDelSDAOut	0
//	10:9	TrimDelSDAIn	1
//	8		ZSeries			0
//	-----------------------------------------------------------

#define	MLX90395_REG_CONFIG0_L_DEFAULT	0b10010000

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

#endif
