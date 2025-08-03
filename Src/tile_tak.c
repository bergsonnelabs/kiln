#include "tile_tak.h"

I2C_HandleTypeDef* tak_handle;

uint8_t return_reg = 0x00;

uint8_t RX_Buffer[2] = {0,0};
int *rx_ptr = (int *)RX_Buffer;
uint8_t TX_Buffer[2] = {0,0};
int *tx_ptr = (int *)TX_Buffer;

void bos1921_write(uint8_t index, uint8_t reg, uint16_t value);
void bos1921_set_return_reg(uint8_t index, uint8_t reg);

uint8_t tile_tak_init(I2C_HandleTypeDef* hi2c, uint8_t index, uint16_t gpio_pin)
{
	uint16_t temp = 0;
	tak_handle = hi2c;

	// wake the chip
	HAL_I2C_Mem_Write(tak_handle, BOS1921_I2C_ADDR<<1, 0x00, 1, (uint8_t *)TX_Buffer, 2, 1000);
//	HAL_Delay(50);

	tile_tak_reset(0);

	// 6 = 1
	// 5 = 0
	// 4:0 = 11110 (0x1E)
//	bos1921_write(0,BOS_REG_COMM,0x005E); // GPIODIR = 1; RDARRR = 0x1E

	// A8 = far; A12 = near
//    HAL_GPIO_WritePin(GPIOA, gpio_pin, 0);

    // SUP RISE
    // 15:12	0100	I2C ADDR LSB
    // 11		1		LP
    // 10:6		00101	VDD
    // 5:0		100111	TI_RISE
    // DEFAULT: 0100100101100111 (0x4967)
    // NEW: change I2C ADDR LSB from 4 to 5
//    bos1921_write(0,BOS_REG_SUP_RISE, (0x4967 | ((uint16_t)index)<<12) );
//	*tx_ptr = __builtin_bswap16(0x4967 | ((uint16_t)index)<<12);
//	HAL_I2C_Mem_Write(tak_handle, BOS1921_I2C_ADDR<<1, BOS_REG_SUP_RISE, 1, (uint8_t *)TX_Buffer, 2, 1000);

	temp = tile_tak_read(index);
	if( (temp & 0x0FFF) != 0x0781){

		return 0;
	}
	return 1;
}

void tile_tak_reset(uint8_t index){
	bos1921_write(index, BOS_REG_CONFIG, 0x0040);
//	HAL_Delay(50);
}

void tile_tak_set_mode(uint8_t index, uint8_t mode){
	switch(mode){
		default:
        case TILE_TAK_MODE_IDLE:
            bos1921_write(index,BOS_REG_CONFIG, 0x0000);
            bos1921_set_return_reg(index,BOS_REG_IC_STATUS);
            break;
        case TILE_TAK_MODE_SENSE_FINE:
            bos1921_write(index,BOS_REG_CONFIG, 0x3010);
            // SENSE = 1 > enable sensing
            // GAINS = 1 > sense resolution LSB = 7.6mV
            // OE = 1 > enable output
            bos1921_set_return_reg(index,BOS_REG_SENSE_VAL);
            break;
        case TILE_TAK_MODE_SENSE_COARSE:
            bos1921_write(index,BOS_REG_CONFIG, 0x2010);
            // SENSE = 1 > enable sensing
            // GAINS = 0 > sense resolution LSB = 54.5mV
            // OE = 1 > enable output
            bos1921_set_return_reg(index,BOS_REG_SENSE_VAL);
            break;
        case TILE_TAK_MODE_PLAY_FIFO:
            bos1921_write(index,BOS_REG_CONFIG, 0x0217);
            // OE = 1 > enable output
            // PLAY_MODE = 1 > FIFO
            // PLAY_SRATE = 7 > 8ksps
            bos1921_set_return_reg(index,BOS_REG_IC_STATUS);
            break;
        case TILE_TAK_MODE_DEBUG:
            bos1921_set_return_reg(index,BOS_REG_CONFIG);
        	break;
	}
}


uint16_t tile_tak_read(uint8_t index){
	uint8_t RX_Buffer[2] = {0,0};
	int *rx_ptr = (int *)RX_Buffer;

	HAL_I2C_Mem_Read(tak_handle, (0x44+index)<<1, 0x00, 1, (uint8_t *)RX_Buffer, 2, 1000);

	return __builtin_bswap16(*rx_ptr);
}

void tile_tak_write(uint8_t index, uint8_t reg, uint16_t value){
	*tx_ptr = __builtin_bswap16(value);
	HAL_I2C_Mem_Write(tak_handle, (0x44+index)<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

void bos1921_write(uint8_t index, uint8_t reg, uint16_t value){
	*tx_ptr = __builtin_bswap16(value);
	HAL_I2C_Mem_Write(tak_handle, (0x44+index)<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

void bos1921_set_return_reg(uint8_t index, uint8_t reg){
    bos1921_write(index, BOS_REG_COMM, reg);
    return_reg = reg;
}


