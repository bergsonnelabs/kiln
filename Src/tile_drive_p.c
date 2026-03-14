#include "tile_drive_p.h"

I2C_HandleTypeDef* drive_p_handle;

uint8_t drive_p_return_reg = 0x00;

static void bos1921_write(I2C_HandleTypeDef* hi2c, uint8_t reg, uint16_t value);
static void bos1921_set_return_reg(I2C_HandleTypeDef* hi2c, uint8_t reg);

uint8_t tile_drive_p_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, BOS1921_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}

}

uint8_t tile_drive_p_init(I2C_HandleTypeDef* hi2c)
{
	uint8_t TX_Buffer[2] = {0,0};
	uint16_t temp = 0;
//	drive_p_handle = hi2c;

	// attempt to wake the chip
	HAL_I2C_Mem_Write(hi2c, BOS1921_I2C_ADDR<<1, 0x00, 1, (uint8_t *)TX_Buffer, 2, 1000);

	// reset
	tile_drive_p_reset(hi2c);

	// read the CHIP_ID register (default response)
	temp = tile_drive_p_read(hi2c);
	if( (temp & 0x0FFF) != 0x0781){
		return 0;
	}
	return 1;
}

void tile_drive_p_reset(I2C_HandleTypeDef* hi2c){
	bos1921_write(hi2c, BOS_REG_CONFIG, 0x0040);
//	HAL_Delay(50);
}

void tile_drive_p_set_mode(I2C_HandleTypeDef* hi2c, uint8_t mode){
	switch(mode){
		default:
        case TILE_DRIVE_P_MODE_IDLE:
            bos1921_write(hi2c,BOS_REG_CONFIG, 0x0000);
            bos1921_set_return_reg(hi2c,BOS_REG_IC_STATUS);
            break;
        case TILE_DRIVE_P_MODE_SENSE_FINE:
            bos1921_write(hi2c,BOS_REG_CONFIG, 0x3010);
            // SENSE = 1 > enable sensing
            // GAINS = 1 > sense resolution LSB = 7.6mV
            // OE = 1 > enable output
            bos1921_set_return_reg(hi2c,BOS_REG_SENSE_VAL);
            break;
        case TILE_DRIVE_P_MODE_SENSE_COARSE:
            bos1921_write(hi2c,BOS_REG_CONFIG, 0x2010);
            // SENSE = 1 > enable sensing
            // GAINS = 0 > sense resolution LSB = 54.5mV
            // OE = 1 > enable output
            bos1921_set_return_reg(hi2c,BOS_REG_SENSE_VAL);
            break;
        case TILE_DRIVE_P_MODE_PLAY_FIFO:
            bos1921_write(hi2c,BOS_REG_CONFIG, 0x0217);
            // OE = 1 > enable output
            // PLAY_MODE = 1 > FIFO
            // PLAY_SRATE = 7 > 8ksps
            bos1921_set_return_reg(hi2c,BOS_REG_IC_STATUS);
            break;
        case TILE_DRIVE_P_MODE_DEBUG:
            bos1921_set_return_reg(hi2c,BOS_REG_CONFIG);
        	break;
	}
}


uint16_t tile_drive_p_read(I2C_HandleTypeDef* hi2c){
	uint8_t RX_Buffer[2] = {0,0};
	int *rx_ptr = (int *)RX_Buffer;

	HAL_I2C_Mem_Read(hi2c, BOS1921_I2C_ADDR<<1, 0x00, 1, (uint8_t *)RX_Buffer, 2, 1000);

	return __builtin_bswap16(*rx_ptr);
}

void tile_drive_p_write(I2C_HandleTypeDef* hi2c, uint8_t reg, uint16_t value){
	uint8_t TX_Buffer[2] = {0,0};
	int *tx_ptr = (int *)TX_Buffer;

	*tx_ptr = __builtin_bswap16(value);
	HAL_I2C_Mem_Write(hi2c, BOS1921_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

void bos1921_write(I2C_HandleTypeDef* hi2c, uint8_t reg, uint16_t value){
	uint8_t TX_Buffer[2] = {0,0};
	int *tx_ptr = (int *)TX_Buffer;

	*tx_ptr = __builtin_bswap16(value);
	HAL_I2C_Mem_Write(hi2c, BOS1921_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

void bos1921_set_return_reg(I2C_HandleTypeDef* hi2c, uint8_t reg){
    bos1921_write(hi2c, BOS_REG_COMM, reg);
    drive_p_return_reg = reg;
}


