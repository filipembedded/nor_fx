#ifndef STM32F4X_PORT_H
#define STM32F4X_PORT_H

#include "nor_fx.h"

typedef struct {
    void    *hspi;      /* SPI_HandleTypeDef* — void* to avoid HAL header in .h */
    void    *cs_port;   /* GPIO_TypeDef*       — void* for the same reason        */
    uint16_t cs_pin;
} stm32f4x_port_context_t;

enum norfx_status spi_chip_select(void *context);
enum norfx_status spi_chip_deselect(void *context);
enum norfx_status spi_write(void *context, uint8_t *data, uint16_t size);
enum norfx_status spi_read(void *context, uint8_t *data, uint16_t size);
uint32_t get_tick_ms(void *context);
void delay_ms(void *context, uint32_t delay);

#endif // STM32F4X_PORT_H