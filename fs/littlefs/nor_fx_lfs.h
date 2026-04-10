#ifndef NOR_FLASH_LFS_H
#define NOR_FLASH_LFS_H

#include <stdint.h>
#include "lfs.h"
#include "nor_fx.h"

/**
 * @brief Context passed as lfs_config.context to all adapter callbacks.
 *
 * Allocate one instance per filesystem mount and populate @p dev before
 * calling lfs_format() or lfs_mount().
 */
typedef struct {
    struct norfx_device *dev;           /**< Initialised driver handle. */
    uint8_t scratch[NORFX_SECTOR_SIZE]; /**< Sector-sized scratch buffer. */
} norfx_lfs_ctx_t;

int norfx_lfs_read(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, void *buffer, lfs_size_t size);

int norfx_lfs_prog(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, const void *buffer, lfs_size_t size);

int norfx_lfs_erase(const struct lfs_config *c, lfs_block_t block);

int norfx_lfs_sync(const struct lfs_config *c);


#endif // NOR_FLASH_LFS_H
