#ifndef NOR_FX_H
#define NOR_FX_H

#include <stdint.h>
#include <stddef.h>

#define NORFX_WIP_MASK                  0x01u
#define NORFX_RESET_TIMEOUT_MS          50u
#define NORFX_WRITE_EN_TIMEOUT_MS       10u
#define NORFX_SECTOR_ERASE_TIMEOUT_MS   400u
#define NORFX_BLOCK_ERASE_TIMEOUT_MS    2000u
#define NORFX_PAGE_PROGRAM_TIMEOUT_MS   3u

enum norfx_status {
	NORFX_SUCCESS = 0,
	NORFX_ERROR = 1,
	NORFX_READY = 2,
	NORFX_BUSY = 3,
	NORFX_TIMEOUT = 4,
    NORFX_ENODEV = 5,
    NORFX_EINVAL = 6
};

enum norfx_instruction {
    INST_WRITE_ENABLE = 0x06,
    INST_VOLATILE_SR_WRITE_ENABLE = 0x50,
    INST_WRITE_DISABLE = 0x04,

    INST_RELEASE_POWER_DOWN_ID = 0xAB,
    INST_MANUFACTURER_DEVICE_ID = 0x90,
    INST_JEDEC_ID = 0x9F,
    INST_READ_UNIQUE_ID = 0x4B,

    INST_READ_DATA = 0x03,
    INST_FAST_READ = 0x0B,
        
    INST_PAGE_PROGRAM = 0x02,

    INST_SECTOR_ERASE_4KB = 0x20,
    INST_BLOCK_ERASE_32KB = 0x52,
    INST_BLOCK_ERASE_64KB = 0xD8,
    INST_CHIP_ERASE = 0xC7, // 0x60

    INST_READ_STATUS_REG_1 = 0x05,
    INST_WRITE_STATUS_REG_1 = 0x01,
    INST_READ_STATUS_REG_2 = 0x35,
    INST_WRITE_STATUS_REG_2 = 0x31,
    INST_READ_STATUS_REG_3 = 0x15,
    INST_WRITE_STATUS_REG_3 = 0x11,

    INST_READ_SFDP_REG = 0x5A,
    INST_ERASE_SECURITY_REG = 0x44,
    INST_PROGRAM_SECURITY_REG = 0x42,
    INST_READ_SECURITY_REG = 0x48,

    INST_GLOBAL_BLOCK_LOCK = 0x7E,
    INST_GLOBAL_BLOCK_UNLOCK = 0x98,
    INST_READ_BLOCK_LOCK = 0x3D,
    INST_INDIVIDUAL_BLOCK_LOCK = 0x36,
    INST_INDIVIDUAL_BLOCK_UNLOCK = 0x39,

    INST_ERASE_PROGRAM_SUSPEND = 0x75,
    INST_ERASE_PROGRAM_RESUME = 0x7A,
    INST_POWER_DOWN = 0xB9,
        
    INST_ENABLE_RESET = 0x66,
    INST_RESET_DEVICE = 0x99,
};

enum norfx_id_kind {
    ID_RELEASE_POWER_DOWN = 0,
    ID_MANUFACTURER_DEVICE = 1,
    ID_JEDEC = 2,
    ID_READ_UNIQUE = 3,
};

struct norfx_device {
	void *context;
	enum norfx_status(*spi_chip_select)(void *context);
	enum norfx_status(*spi_chip_deselect)(void *context);
	enum norfx_status(*spi_write)(void *context, uint8_t *data, uint16_t size);
	enum norfx_status(*spi_read)(void *context, uint8_t *data, uint16_t size);
    uint32_t (*get_tick_ms)(void *context);
    void (*delay_ms)(void *context, uint32_t delay);
};

enum norfx_status norfx_reset(struct norfx_device *dev);
enum norfx_status norfx_read_id(struct norfx_device *dev, enum norfx_id_kind id, uint32_t *id_val);
enum norfx_status norfx_read(struct norfx_device *dev,
                            uint32_t start_page,
                            uint8_t offset,
                            uint32_t size,
                            uint8_t *rx_buf);
enum norfx_status norfx_fast_read(struct norfx_device *dev,
                            uint32_t start_page,
                            uint8_t offset,
                            uint32_t size,
                            uint8_t *rx_buf);

enum norfx_status norfx_write_enable(struct norfx_device *dev);
enum norfx_status norfx_write_disable(struct norfx_device *dev);
enum norfx_status norfx_erase_sector(struct norfx_device *dev, uint16_t num_sector);
enum norfx_status norfx_read_status_reg(struct norfx_device *dev, uint8_t *status_reg);

#endif // NOR_FX_H
