#include "nor_fx.h"
#include <stdint.h>

static uint32_t calculate_bytes_to_write(uint32_t size, uint16_t offset);
static uint32_t calculate_bytes_to_modify(uint32_t size, uint16_t offset);

enum norfx_status norfx_reset(struct norfx_device *dev)
{
    //TODO: Impl
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

uint8_t norfx_read_status_reg(struct norfx_device *dev)
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
