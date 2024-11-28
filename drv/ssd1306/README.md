# ssd1306
simple SSD130x driver based on https://github.com/afiskon/stm32-ssd1306.

HAL dependencies removed, transport layer moved to weak functions. you need to provide:

- `ssd1306_Reset(void)` -> toggle reset pin with CS high (for SPI use), 10ms delay
- `ssd1306_WriteSingle(uint8_t)`
- `ssd1306_WriteMulti(uint8_t *, size_t)` 
- `ssd1306_delayMs(uint32_t)`
