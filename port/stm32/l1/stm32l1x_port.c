#include "stm32l1x_port.h"
#include "stm32l1xx_hal.h"

#define SPI_TIMEOUT_MS 50

enum norfx_status spi_chip_select(void *context)
{
    enum norfx_status status = NORFX_ERROR; 
    stm32l1x_port_context_t *port_context;
    if (context == NULL)
    {
        status = NORFX_EINVAL;
    }
    else
    {
        port_context = (stm32l1x_port_context_t *)context;
        HAL_GPIO_WritePin(port_context->cs_port, port_context->cs_pin, GPIO_PIN_RESET);
        status = NORFX_SUCCESS;
    }

    return status;
}

enum norfx_status spi_chip_deselect(void *context)
{
    enum norfx_status status = NORFX_ERROR;
    stm32l1x_port_context_t *port_context; 
    if (context == NULL)
    {
        status = NORFX_EINVAL;
    }
    else
    {
        port_context = (stm32l1x_port_context_t *)context;
        HAL_GPIO_WritePin(port_context->cs_port, port_context->cs_pin, GPIO_PIN_SET);
        status = NORFX_SUCCESS;
    }

    return status;
}

enum norfx_status spi_write(void *context, uint8_t *data, uint16_t size)
{
    if (context == NULL || data == NULL || size == 0)
    {
        return NORFX_EINVAL;
    }

    stm32l1x_port_context_t *port_context = (stm32l1x_port_context_t *)context;

    HAL_StatusTypeDef h_status = HAL_SPI_Transmit(port_context->hspi, data, size, SPI_TIMEOUT_MS);
    if (h_status != HAL_OK)
    {
        return NORFX_ERROR;
    }

    return NORFX_SUCCESS;
}

enum norfx_status spi_read(void *context, uint8_t *data, uint16_t size)
{
    if (context == NULL || data == NULL || size == 0)
    {
        return NORFX_EINVAL;
    }

    stm32l1x_port_context_t *port_context = (stm32l1x_port_context_t *)context;

    HAL_StatusTypeDef h_status = HAL_SPI_Receive(port_context->hspi, data, size, SPI_TIMEOUT_MS);
    if (h_status != HAL_OK)
    {
        return NORFX_ERROR;
    }

    return NORFX_SUCCESS;
}

uint32_t get_tick_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

void delay_ms(void *context, uint32_t delay)
{
    (void)context;
    HAL_Delay(delay);
}