#include "tile_tak.h"

I2C_HandleTypeDef* tak_handles[10];

uint8_t return_reg = 0x00;

uint8_t RX_Buffer[2] = {0,0};
int *rx_ptr = (int *)RX_Buffer;
uint8_t TX_Buffer[2] = {0,0};
int *tx_ptr = (int *)TX_Buffer;

void bos1921_write(uint8_t index, uint8_t reg, uint16_t value);
void bos1921_set_return_reg(uint8_t index, uint8_t reg);

uint8_t tile_tak_find(I2C_HandleTypeDef* hi2c, uint8_t index)
{
	uint16_t temp = 0;
	tak_handles[index] = hi2c;

	// SEND DUMMY DATA TO WAKE
	HAL_I2C_Mem_Write(tak_handles[index], (BOS1921_I2C_ADDR+index)<<1, 0x00, 1, (uint8_t *)TX_Buffer, 2, 1000);

	tile_tak_reset(index);

	temp = tile_tak_read(index);
	if( (temp & 0x0FFF) != 0x0781){
		return 0;
	}
	return 1;

}

uint8_t tile_tak_init(uint8_t index)
{
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

	HAL_I2C_Mem_Read(tak_handles[index], (BOS1921_I2C_ADDR+index)<<1, 0x00, 1, (uint8_t *)RX_Buffer, 2, 1000);

	return __builtin_bswap16(*rx_ptr);
}

void tile_tak_write(uint8_t index, uint8_t reg, uint16_t value){
	*tx_ptr = __builtin_bswap16(value);
	HAL_I2C_Mem_Write(tak_handles[index], (BOS1921_I2C_ADDR+index)<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

void bos1921_write(uint8_t index, uint8_t reg, uint16_t value){
	*tx_ptr = __builtin_bswap16(value);
	HAL_I2C_Mem_Write(tak_handles[index], (BOS1921_I2C_ADDR+index)<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

void bos1921_set_return_reg(uint8_t index, uint8_t reg){
    bos1921_write(index, BOS_REG_COMM, reg);
    return_reg = reg;
}


