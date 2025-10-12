#include "tile_sense_bp.h"

static I2C_HandleTypeDef* tile_handle;
static uint32_t pressure_offset = 0;

// ---------------------------------------------------------
// PRIVATE FUNCTIONS
// ---------------------------------------------------------
uint8_t ilps22qs_read(uint8_t reg){
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(tile_handle, ILPS22QS_I2C_ADDR<<1, reg, 1, (uint8_t *)RX_Buffer, 1, 1000);
	return RX_Buffer[0];
}

void ilps22qs_write(uint8_t reg, uint8_t value){
	uint8_t TX_Buffer[1] = {value};
	HAL_I2C_Mem_Write(tile_handle, ILPS22QS_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 1, 1000);
}

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_bp_check(I2C_HandleTypeDef* hi2c)
{
	uint8_t RX_Buffer[1];
	HAL_I2C_Mem_Read(hi2c, ILPS22QS_I2C_ADDR<<1, ILPS22QS_REG_WHO_AM_I, 1, (uint8_t *)RX_Buffer, 1, 1000);
	if(RX_Buffer[0] == ILPS22QS_REG_WHO_AM_I_DEFAULT){
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_bp_init(I2C_HandleTypeDef* hi2c)
{
	tile_handle = hi2c;

	if(!tile_sense_bp_check(tile_handle)){
		return 0;
	}

	// set the ODR>0 to wake from power-down state
	ilps22qs_write(ILPS22QS_REG_CTRL_REG1, 0b00101000); // 50Hz ODR, 4-point averaging

	ilps22qs_write(ILPS22QS_REG_CTRL_REG2, 0b00010000); // set full scale to 1260hPA

	tile_sense_bp_set_offset();
	return 1;
}

uint32_t tile_sense_bp_get_pressure(void){
    return (uint32_t) (
    	( ( (uint32_t) ilps22qs_read(ILPS22QS_REG_PRESS_OUT_H) )<<16) +
		( ( (uint32_t) ilps22qs_read(ILPS22QS_REG_PRESS_OUT_L) )<<8) +
		( (uint32_t) ilps22qs_read(ILPS22QS_REG_PRESS_OUT_XL) )
		);
}

void tile_sense_bp_set_offset(void){
	pressure_offset = tile_sense_bp_get_pressure();
}

int16_t tile_sense_bp_get_scaled_pressure(void){ // returns Pa
	return (int16_t) ( (tile_sense_bp_get_pressure() - pressure_offset) / 40.960);
}
