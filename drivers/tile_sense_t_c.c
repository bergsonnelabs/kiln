#include "tile_sense_t_c.h"

static I2C_HandleTypeDef* t_c_handle;


void t_c_write(uint8_t reg, uint16_t value){

	uint8_t TX_Buffer[2] = {0,0};
	int *tx_ptr = (int *)TX_Buffer;

	*tx_ptr = value; // = __builtin_bswap16(value);

	HAL_I2C_Mem_Write(t_c_handle, IQS323_I2C_ADDR<<1, reg, 1, (uint8_t *)TX_Buffer, 2, 1000);
}

uint16_t t_c_read(uint8_t reg)
{

}

// ---------------------------------------------------------
// PUBLIC FUNCTIONS
// ---------------------------------------------------------

uint8_t tile_sense_t_c_find(I2C_HandleTypeDef* hi2c)
{
	if(HAL_I2C_IsDeviceReady(hi2c, IQS323_I2C_ADDR<<1, 3, 1000) == 0){ // HAL_StatusTypeDef = 0=okay, 1=error, 2=busy, 3=timeout
		return 1;
	} else {
		return 0;
	}
}

uint8_t tile_sense_t_c_init(I2C_HandleTypeDef* hi2c)
{
	int i = 0;

	t_c_handle = hi2c;
	// SYSTEM CONTROL : SOFT RESET

	// SOFT RESET
	t_c_write(IQS323_REG_SYSTEM_CONTROL,0x02);

	// WAIT FOR RESET FLAG (B7)

	t_c_read(IQS323_REG_SYSTEM_STATUS);

	// ACKNOWLEDGE RESET
	t_c_write(IQS323_REG_SYSTEM_CONTROL, 0x01);

	// SENSOR SETUP : ENABLE CHANNEL
	// PROX CONTROL: PXS MODE
	// PROX INPUT AND CONTROL + SENSOR SETUP : RXS & TXS
	// CHANNEL SETUP : CHANNEL MODE = INDEPENDENT
	// CONVERSION FREQUENCY (SEC 6.5)
	// PATTERN DEFINITIONS: INACTIVE = VSS


}

uint16_t tile_sense_t_c_get_data(void)
{
//	uint16_t RX_Buffer[1];
//	HAL_I2C_Master_Receive(tile_handle, IQS323_I2C_ADDR<<1, (uint8_t*)RX_Buffer, 2, 1000);
//	return (0x0FFF & __builtin_bswap16(RX_Buffer[0]));
}
