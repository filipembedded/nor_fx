#include "nor_fx.h"
#include <stdint.h>
#include <stdbool.h>

static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset);
static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset);
static enum norfx_status check_flash_ready(struct norfx_device *dev);
static enum norfx_status check_id_kind(enum norfx_id_kind id);
static uint8_t get_id_size(enum norfx_id_kind id);
static enum norfx_status convert_buf_to_id(uint8_t *rx_buf, enum norfx_id_kind id, uint32_t *id_val);

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
    uint8_t rx_buf[12] = {0};   /* max: 4 dummy + 8 unique ID bytes */
    uint8_t rx_len = 0;

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
            /* Returns: MFR_ID | MEM_TYPE | CAPACITY (3 bytes) */
            tx_buf = INST_JEDEC_ID;
            rx_len = get_id_size(ID_JEDEC);
            break;

        case ID_MANUFACTURER_DEVICE:
            /* Requires 3 dummy address bytes, returns MFR_ID | DEV_ID (2 bytes) */
            tx_buf = INST_MANUFACTURER_DEVICE_ID;
            rx_len = get_id_size(ID_MANUFACTURER_DEVICE);   /* 3 dummy addr + 2 data */
            break;

        case ID_RELEASE_POWER_DOWN:
            /* Requires 3 dummy address bytes, returns DEV_ID (1 byte) */
            tx_buf = INST_RELEASE_POWER_DOWN_ID;
            rx_len = get_id_size(ID_RELEASE_POWER_DOWN);   /* 3 dummy addr + 1 data */
            break;

        case ID_READ_UNIQUE:
            /* Requires 4 dummy bytes, returns 64-bit unique ID (8 bytes) */
            tx_buf = INST_READ_UNIQUE_ID;
            rx_len = get_id_size(ID_READ_UNIQUE);  /* 4 dummy + 8 unique */
            break;

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

    if (dev->spi_read(dev->context, rx_buf, rx_len) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        return NORFX_ERROR;
    }

    if (dev->spi_chip_deselect(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    if (convert_buf_to_id(rx_buf, id, id_val) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    return NORFX_SUCCESS;
}

enum norfx_status norfx_read(struct norfx_device *dev,
                            uint32_t start_page,
                            uint8_t offset,
                            uint32_t size,
                            uint8_t *rx_buf)
{
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

    if (rx_buf == NULL)
    {
        return NORFX_EINVAL;
    }

    uint8_t tx_buf[4];
    uint32_t mem_addr = (start_page * 256) + offset;
    tx_buf[0] = INST_READ_DATA;
    tx_buf[1] = (mem_addr >> 16) & 0xFF; // MSB of 24-bit memory address
    tx_buf[2] = (mem_addr >> 8) & 0xFF;
    tx_buf[3] = (mem_addr) & 0xFF;       // LSB of 24-bit memory address
  
    if (dev->spi_chip_select(dev->context) != NORFX_SUCCESS)
    {
        return NORFX_ERROR;
    }

    if (dev->spi_write(dev->context, tx_buf, sizeof(tx_buf)) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        return NORFX_ERROR;
    }

    if (dev->spi_read(dev->context, rx_buf, size) != NORFX_SUCCESS)
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

static uint8_t get_id_size(enum norfx_id_kind id)
{
    uint8_t id_size = 0;
    switch(id)
    {
        case ID_RELEASE_POWER_DOWN:
            id_size = 4u;
            break;
        case ID_MANUFACTURER_DEVICE:
            id_size = 5u;
            break;
        case ID_JEDEC:
            id_size = 3u;
            break;
        case ID_READ_UNIQUE:
            id_size = 12u;
            break;

        default:
            break;
    }

    return id_size;
}

static enum norfx_status convert_buf_to_id(uint8_t *rx_buf, enum norfx_id_kind id, uint32_t *id_val)
{
    if (rx_buf == NULL || id_val == NULL)
    {
        return NORFX_EINVAL;
    }

    switch(id)
    {
        case ID_JEDEC:
            /* ID_JEDEC: MFR_ID | MEM_TYPE | CAPACITY */
            *id_val = ((uint32_t)rx_buf[0] << 16) |
                      ((uint32_t)rx_buf[1] << 8)  |
                      ((uint32_t)rx_buf[2]);
            break;

        case ID_RELEASE_POWER_DOWN:
            /* ID_RELEASE_POWER_DOWN: 3 dummy addr + DEV_ID at [3] */
            *id_val = (uint32_t)rx_buf[3];
            break;

        case ID_MANUFACTURER_DEVICE:
            /* ID_MANUFACTURER_DEVICE: 3 dummy addr + MFR_ID | DEV_ID */
            *id_val = ((uint32_t)rx_buf[3] << 8) |
                      ((uint32_t)rx_buf[4]);
            break;

        case ID_READ_UNIQUE:
            /* ID_READ_UNIQUE: 4 dummy + 8-byte unique ID, packed into 32 LSBs */
            *id_val = ((uint32_t)rx_buf[8]  << 24) |
                      ((uint32_t)rx_buf[9]  << 16) |
                      ((uint32_t)rx_buf[10] << 8)  |
                      ((uint32_t)rx_buf[11]);
            break;

        default:
            return NORFX_EINVAL;
    }

    return NORFX_SUCCESS;
}