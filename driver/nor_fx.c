/**
 * @file nor_fx.c
 * @brief Cross-platform NOR flash driver — implementation.
 *
 * All public functions are documented in nor_fx.h.
 * Static helper functions are documented here at the point of definition.
 */

#include "nor_fx.h"
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Static forward declarations
 * ------------------------------------------------------------------------- */
static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset);
static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset);
static enum norfx_status check_flash_ready(struct norfx_device *dev, uint32_t timeout_ms);
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

    enum norfx_status status = check_flash_ready(dev, NORFX_RESET_TIMEOUT_MS);
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

    enum norfx_status status = check_flash_ready(dev, NORFX_WRITE_EN_TIMEOUT_MS);
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

    enum norfx_status status = check_flash_ready(dev, NORFX_WRITE_EN_TIMEOUT_MS);
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

    uint8_t tx_buf[4] = {0};
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

enum norfx_status norfx_fast_read(struct norfx_device *dev,
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

    uint8_t tx_buf[5] = {0};
    uint32_t mem_addr = (start_page * 256) + offset;
    tx_buf[0] = INST_FAST_READ;
    tx_buf[1] = (mem_addr >> 16) & 0xFF; // MSB of 24-bit memory address
    tx_buf[2] = (mem_addr >> 8) & 0xFF;
    tx_buf[3] = (mem_addr) & 0xFF;       // LSB of 24-bit memory address
    tx_buf[4] = 0x00;

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

enum norfx_status norfx_erase_sector(struct norfx_device *dev, uint16_t num_sector)
{
    enum norfx_status status;
    uint8_t tx_buf[4] = {0};

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

    // Sector contains 16 pages, page contains 256 bytes
    uint32_t mem_addr = num_sector*16*256;

    status = norfx_write_enable(dev);
    if (status != NORFX_SUCCESS)
    {
        return status;
    }

    tx_buf[0] = INST_SECTOR_ERASE_4KB;
    tx_buf[1] = (mem_addr >> 16) & 0xFF; // MSB of a 24-bit memory address
    tx_buf[2] = (mem_addr >> 8) & 0xFF;
    tx_buf[3] = (mem_addr) & 0xFF;       // LSB of a 24-bit memory address

    if (dev->spi_chip_select(dev->context) != NORFX_SUCCESS)
    {
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    if (dev->spi_write(dev->context, tx_buf, sizeof(tx_buf)) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    if (dev->spi_chip_deselect(dev->context) != NORFX_SUCCESS)
    {
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    status = check_flash_ready(dev, NORFX_SECTOR_ERASE_TIMEOUT_MS);
    if (status != NORFX_SUCCESS)
    {
        (void)norfx_write_disable(dev);
        return status;
    }

    status = norfx_write_disable(dev);
    if (status != NORFX_SUCCESS)
    {
        return status;
    }

    return NORFX_SUCCESS;
}

enum norfx_status norfx_page_program(struct norfx_device *dev,
                                    uint32_t page,
                                    uint16_t offset,
                                    uint32_t size,
                                    uint8_t *data)
{
    enum norfx_status status;
    uint8_t tx_buf[4] = {0};

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

    if (data == NULL || size == 0u || offset > 255u)
    {
        return NORFX_EINVAL;
    }

    uint32_t mem_addr = (page * 256u) + offset;
    tx_buf[0] = INST_PAGE_PROGRAM;
    tx_buf[1] = (mem_addr >> 16) & 0xFFu; // MSB of 24-bit memory address
    tx_buf[2] = (mem_addr >> 8)  & 0xFFu;
    tx_buf[3] = (mem_addr)       & 0xFFu; // LSB of 24-bit memory address

    status = norfx_write_enable(dev);
    if (status != NORFX_SUCCESS)
    {
        return status;
    }

    if (dev->spi_chip_select(dev->context) != NORFX_SUCCESS)
    {
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    /* Send 4-byte header: opcode + 24-bit address */
    if (dev->spi_write(dev->context, tx_buf, sizeof(tx_buf)) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    /* Send data directly — no copy into intermediate buffer */
    if (dev->spi_write(dev->context, data, (uint16_t)size) != NORFX_SUCCESS)
    {
        (void)dev->spi_chip_deselect(dev->context);
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    if (dev->spi_chip_deselect(dev->context) != NORFX_SUCCESS)
    {
        (void)norfx_write_disable(dev);
        return NORFX_ERROR;
    }

    status = check_flash_ready(dev, NORFX_PAGE_PROGRAM_TIMEOUT_MS);
    if (status != NORFX_SUCCESS)
    {
        (void)norfx_write_disable(dev);
        return status;
    }

    status = norfx_write_disable(dev);
    if (status != NORFX_SUCCESS)
    {
        return status;
    }

    return NORFX_SUCCESS;
}


enum norfx_status norfx_write(struct norfx_device *dev,
                              uint32_t page,
                              uint16_t offset,
                              uint32_t size,
                              uint8_t *data,
                              uint8_t *scratch_buf)
{
    enum norfx_status status;

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

    if (data == NULL || scratch_buf == NULL || size == 0u)
    {
        return NORFX_EINVAL;
    }

    uint16_t start_sector = (uint16_t)(page / 16u);
    uint16_t end_sector   = (uint16_t)((page + ((size + offset - 1u) / 256u)) / 16u);
    uint16_t num_sectors  = end_sector - start_sector + 1u;

    uint32_t sector_offset = ((page % 16u) * 256u) + offset;
    uint32_t data_index    = 0u;

    for (uint16_t i = 0u; i < num_sectors; i++)
    {
        uint32_t start_page = (uint32_t)start_sector * 16u;

        /* 1. Read full sector into scratch buffer */
        status = norfx_fast_read(dev, start_page, 0u, 4096u, scratch_buf);
        if (status != NORFX_SUCCESS)
        {
            return status;
        }

        /* 2. Overlay new data onto the scratch buffer */
        uint32_t bytes_to_modify = calculate_bytes_to_modify(size, (uint16_t)sector_offset);
        for (uint32_t j = 0u; j < bytes_to_modify; j++)
        {
            scratch_buf[j + sector_offset] = data[j + data_index];
        }

        /* 3. Erase sector */
        status = norfx_erase_sector(dev, start_sector);
        if (status != NORFX_SUCCESS)
        {
            return status;
        }

        /* 4. Write sector back page by page */
        for (uint32_t p = 0u; p < 16u; p++)
        {
            status = norfx_page_program(dev, start_page + p, 0u, 256u,
                                        &scratch_buf[p * 256u]);
            if (status != NORFX_SUCCESS)
            {
                return status;
            }
        }

        start_sector++;
        sector_offset  = 0u;
        data_index    += bytes_to_modify;
        size          -= (uint32_t)bytes_to_modify;
    }

    return NORFX_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * Static helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Calculate how many bytes fit in the current page.
 *
 * @param size    Remaining bytes to write.
 * @param offset  Current byte offset within a 256-byte page.
 * @return        Number of bytes that can be written without crossing a
 *                page boundary.
 */
static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset)
{
    if ((size + offset) < 256)
        return size; 
    else 
        return (256 - offset);
}

/**
 * @brief Calculate how many bytes fall within the current sector.
 *
 * @param size    Remaining bytes to modify.
 * @param offset  Current byte offset within a 4096-byte sector.
 * @return        Number of bytes that can be modified without crossing a
 *                sector boundary.
 */
static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset)
{
    if ((size + offset) < 4096)
        return size;
    else 
        return (4096 - offset);
}


/**
 * @brief Poll Status Register 1 until WIP clears or the timeout expires.
 *
 * Reads SR1 in a loop, sleeping 1 ms between polls. The timeout is
 * wraparound-safe as long as the elapsed time does not exceed UINT32_MAX ms.
 *
 * @param[in] dev         Initialised device handle.
 * @param[in] timeout_ms  Maximum time to wait in milliseconds.
 * @return                @ref NORFX_SUCCESS when WIP == 0,
 *                        @ref NORFX_TIMEOUT if the timeout elapses,
 *                        or any error returned by @ref norfx_read_status_reg.
 */
static enum norfx_status check_flash_ready(struct norfx_device *dev, uint32_t timeout_ms)
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

        if ((dev->get_tick_ms(dev->context) - start_time_ms) > timeout_ms)
        {
            return NORFX_TIMEOUT;
        }

        dev->delay_ms(dev->context, 1u);
    }
}

/**
 * @brief Validate that @p id is a member of @ref norfx_id_kind.
 *
 * @param[in] id  ID kind to validate.
 * @return        @ref NORFX_SUCCESS if valid, @ref NORFX_EINVAL otherwise.
 */
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

/**
 * @brief Return the total SPI receive length for a given ID type.
 *
 * Includes any dummy bytes required before the actual ID payload.
 *
 * @param[in] id  ID kind.
 * @return        Number of bytes to clock in after sending the opcode:
 *                - @ref ID_JEDEC               → 3  (MFR | MemType | Capacity)
 *                - @ref ID_MANUFACTURER_DEVICE → 5  (3 dummy addr + MFR | DevID)
 *                - @ref ID_RELEASE_POWER_DOWN  → 4  (3 dummy addr + DevID)
 *                - @ref ID_READ_UNIQUE         → 12 (4 dummy + 8-byte UID)
 *                - unknown                     → 0
 */
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

/**
 * @brief Extract a 32-bit ID value from a raw SPI receive buffer.
 *
 * Each ID type has a different byte layout; this function handles the
 * correct extraction and packing for all supported ID kinds.
 *
 * @param[in]  rx_buf  Raw bytes received from the device (including dummy bytes).
 * @param[in]  id      ID kind that determines the byte layout.
 * @param[out] id_val  Decoded identifier.
 * @return             @ref NORFX_SUCCESS, or @ref NORFX_EINVAL if any pointer
 *                     is NULL or @p id is unrecognised.
 */
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