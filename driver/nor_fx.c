#include "nor_fx.h"
#include <stdint.h>
#include <stdbool.h>

static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset);
static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset);
static enum norfx_status check_flash_ready(struct norfx_device *dev);
static enum norfx_status check_id_kind(enum norfx_id_kind id);
static enum norfx_status convert_buf_to_id(uint8_t *rx_buf, uint32_t *id_val);

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
        return status;
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

enum norfx_status norfx_write_enable(struct norfx_device *dev)
{
    uint8_t tx_buf = INST_WRITE_ENABLE;

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

    if (dev->spi_write(dev->context, &tx_buf, 1) != NORFX_SUCCESS)
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
        return status;
    }

    return NORFX_SUCCESS;
}

enum norfx_status norfx_write_disable(struct norfx_device *dev)
{
    uint8_t tx_buf = INST_WRITE_DISABLE;

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

    if (dev->spi_write(dev->context, &tx_buf, 1) != NORFX_SUCCESS)
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
        return status;
    }

    return NORFX_SUCCESS;
}

enum norfx_status norfx_read_id(struct norfx_device *dev, enum norfx_id_kind id, uint32_t *id_val)
{
    uint8_t tx_buf = 0;
    uint8_t rx_buf[3] = {0};

    if (dev == NULL)
    {
        return NORFX_ENODEV;
    }

    if (dev->spi_chip_select == NULL ||
        dev->spi_chip_deselect == NULL ||
        dev->spi_write == NULL ||
        dev->spi_read == NULL)
    {
        return NORFX_EINVAL;
    }

    if (id_val == NULL)
    {
        return NORFX_EINVAL;
    }

    if (check_id_kind(id) != NORFX_SUCCESS)
    {
        return NORFX_EINVAL;
    }

    switch(id)
    {
        case ID_JEDEC:
            tx_buf = INST_JEDEC_ID;
            
            break;

        // TODO: Add more ID's here...

        default:
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

    if (dev->spi_read(dev->context, rx_buf, sizeof(rx_buf)) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        return NORFX_ERROR;
    }

    if (dev->spi_chip_deselect(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    if (convert_buf_to_id(rx_buf, id_val) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    return NORFX_SUCCESS;
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

static enum norfx_status check_id_kind(enum norfx_id_kind id)
{
    enum norfx_status status;
    if (id < ID_RELEASE_POWER_DOWN || id > ID_READ_UNIQUE)
    {
        status = NORFX_EINVAL;
    }
    else
    {
        status = NORFX_SUCCESS;
    }

    return status;
}

static enum norfx_status convert_buf_to_id(uint8_t *rx_buf, uint32_t *id_val)
{
    enum norfx_status status = NORFX_ERROR;

    if (rx_buf == NULL || id_val == NULL)
    {
        status = NORFX_EINVAL;
    }
    else
    {
        *id_val = ((rx_buf[0] << 16) | (rx_buf[1] << 8) | (rx_buf[2]));
        status = NORFX_SUCCESS;
    }

    return status;
}