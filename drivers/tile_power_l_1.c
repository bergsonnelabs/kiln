#include <tile_power_l_1.h>

I2C_HandleTypeDef* power_l_1_handle;

uint8_t tile_power_l_1_init(I2C_HandleTypeDef* hi2c){
	uint8_t RX_Buffer[1];
	uint8_t TX_Buffer[1] = {0};

	power_l_1_handle = hi2c;
	HAL_I2C_Mem_Read(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_DEVICE_ID, 1, (uint8_t *)RX_Buffer, 1, 1000);

	if(RX_Buffer[0] != BQ25150_REG_DEVICE_ID_DEFAULT){
		return 0; // did not find the BQ25150
	}

	TX_Buffer[0] = 0x40; // fast charge current to 80mA
	HAL_I2C_Mem_Write(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_ICHG_CTRL, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	TX_Buffer[0] = 0x0F; // precharge current to 20mA
	HAL_I2C_Mem_Write(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_PCHRGCTRL, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	TX_Buffer[0] = 0x04; // BUVLO to 2.6V
	HAL_I2C_Mem_Write(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_BUVLO, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	TX_Buffer[0] = 0x08; // read VBAT
	HAL_I2C_Mem_Write(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_ADC_READ_EN, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	TX_Buffer[0] = 0X82; // 1-sec update rate in battery mode
	HAL_I2C_Mem_Write(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_ADCCTRL0, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	TX_Buffer[0] = 0x00; // disable ship mode
	HAL_I2C_Mem_Write(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_ICCTRL0, I2C_MEMADD_SIZE_8BIT, TX_Buffer, 1, 1000);

	return 1; // success
}


uint16_t tile_power_l_1_get_vbat(void)
{
    uint16_t result = 0;
    uint8_t RX_Buffer[1];
    HAL_I2C_Mem_Read(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_VBAT_MSB, 1, (uint8_t *)RX_Buffer, 1, 1000);
    result = RX_Buffer[0]<<8;
    HAL_I2C_Mem_Read(power_l_1_handle, BQ25150_I2C_ADDR<<1, BQ25150_REG_VBAT_LSB, 1, (uint8_t *)RX_Buffer, 1, 1000);
    result |= RX_Buffer[0];
    return result;

}

uint8_t tile_power_l_1_get_percent(void){
    static uint8_t result = 0;
    uint16_t voltage = tile_power_l_1_get_vbat();
    if(voltage < 38229){ // unplugged
        result = 0;
    } else {
        result = (uint8_t) ((voltage/76.458) - 500);
    }
    if(result > 100){
        result = 100;
    }
    return result;
}
