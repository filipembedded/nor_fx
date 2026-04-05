#include "nor_fx.h"
#include <stdint.h>
#include <stdbool.h>

static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset);
static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset);
static enum norfx_status check_flash_ready(struct norfx_device *dev);

enum norfx_status norfx_reset(struct norfx_device *dev)
{
    uint8_t tx_buf[2] = {0};

    if (dev == NULL)
    {
        return NORFX_ENODEV;
    }

    if (dev->spi_chip_deselect == NULL ||
        dev->spi_chip_select == NULL ||
        dev->spi_write == NULL ||
        dev->spi_read == NULL)
    {
        return NORFX_EINVAL;
    }

    if (dev->spi_chip_select(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    tx_buf[0] = INST_ENABLE_RESET;
    tx_buf[1] = INST_RESET_DEVICE;

    if (dev->spi_write(dev->context, tx_buf, 2) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        return NORFX_ERROR;
    }

    if (dev->spi_chip_deselect(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    enum norfx_status status = check_flash_ready(dev);
    if (status != NORFX_SUCCESS)
    {
        return NORFX_TIMEOUT;
    } 

    return NORFX_SUCCESS;
}

enum norfx_status norfx_read_status_reg(struct norfx_device *dev, uint8_t *status_reg)
{
    uint8_t tx_buf = INST_READ_STATUS_REG_1;

    if (dev == NULL)
    {
        return NORFX_ENODEV;
    }

    if (status_reg == NULL)
    {
        return NORFX_EINVAL;
    }

    if (dev->spi_chip_deselect == NULL ||
        dev->spi_chip_select == NULL ||
        dev->spi_write == NULL ||
        dev->spi_read == NULL)
    {
        return NORFX_EINVAL;
    }

    if (dev->spi_chip_select(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    if (dev->spi_write(dev->context, &tx_buf, 1) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        return NORFX_ERROR;
    }

    if (dev->spi_read(dev->context, status_reg, 1) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        return NORFX_ERROR;
    }

    if (dev->spi_chip_deselect(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    return NORFX_SUCCESS;
}

enum norfx_status norfx_read_id(struct norfx_device *dev, enum norfx_id_kind id)
{
    //TODO: Impl
}

enum norfx_status norfx_read(struct norfx_device *dev,                                     uint32_t start_page,
                            uint8_t offset,
                            uint32_t size,
                            uint8_t *r_data)
{
    //TODO: Impl
}

enum norfx_status norfx_fast_read(struct norfx_device *dev,                                     uint32_t start_page,
                            uint8_t offset,
                            uint32_t size,
                            uint8_t *r_data)
{
    //TODO: Impl
}

enum norfx_status norfx_write_enable(struct norfx_device *dev)
{

}

enum norfx_status norfx_write_disable(struct norfx_device *dev)
{

}

enum norfx_status norfx_erase_sector(struct norfx_device *dev)
{

}





static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset)
{
    if ((size + offset) < 256)
        return size; 
    else 
        return (256 - offset);
}

static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset)
{
    if ((size + offset) < 4096)
        return size;
    else 
        return (4096 - offset);
}


static enum norfx_status check_flash_ready(struct norfx_device *dev)
{
    uint8_t status_reg = 0;
    uint32_t start_time_ms = 0;

    if (dev == NULL)
    {
        return NORFX_ENODEV;
    }

    if (dev->spi_chip_select == NULL ||
        dev->spi_chip_deselect == NULL ||
        dev->spi_write == NULL ||
        dev->spi_read == NULL ||
        dev->get_tick_ms == NULL ||
        dev->delay_ms == NULL)
    {
        return NORFX_EINVAL;
    }

    start_time_ms = dev->get_tick_ms(dev->context);

    while(1)
    {
        enum norfx_status status = norfx_read_status_reg(dev, &status_reg);
        if (status != NORFX_SUCCESS)
        {
            return status;
        }

        if ((status_reg & NORFX_WIP_MASK) == 0u)
        {
            return NORFX_SUCCESS;
        }

        if ((dev->get_tick_ms(dev->context) - start_time_ms) > NORFX_READY_TIMEOUT_MS)
        {
            return NORFX_TIMEOUT;
        }

        dev->delay_ms(dev->context, 1u);
    }
}

