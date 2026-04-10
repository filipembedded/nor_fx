#include "nor_fx_lfs.h"

int norfx_lfs_read(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, void *buffer, lfs_size_t size)
{
    norfx_lfs_ctx_t *ctx = (norfx_lfs_ctx_t *)c->context;

    uint32_t addr   = (uint32_t)(block * c->block_size + (uint32_t)off);
    uint32_t page   = addr / NORFX_PAGE_SIZE;
    uint8_t  offset = (uint8_t)(addr % NORFX_PAGE_SIZE);

    enum norfx_status st = norfx_fast_read(ctx->dev, page, offset,
                                           (uint32_t)size, (uint8_t *)buffer);
    return (st == NORFX_SUCCESS) ? LFS_ERR_OK : LFS_ERR_IO;
}

int norfx_lfs_prog(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, const void *buffer, lfs_size_t size)
{
    norfx_lfs_ctx_t *ctx = (norfx_lfs_ctx_t *)c->context;

    uint32_t addr   = (uint32_t)(block * c->block_size + (uint32_t)off);
    uint32_t page   = addr / NORFX_PAGE_SIZE;
    uint16_t offset = (uint16_t)(addr % NORFX_PAGE_SIZE);

    /* LFS guarantees prog calls are aligned to prog_size (256 B) and never
     * cross a page boundary, so a single norfx_page_program suffices. */
    enum norfx_status st = norfx_page_program(ctx->dev, page, offset,
                                              (uint32_t)size,
                                              (uint8_t *)(uintptr_t)buffer);
    return (st == NORFX_SUCCESS) ? LFS_ERR_OK : LFS_ERR_IO;
}

int norfx_lfs_erase(const struct lfs_config *c, lfs_block_t block)
{
    norfx_lfs_ctx_t *ctx = (norfx_lfs_ctx_t *)c->context;
    (void)c;

    enum norfx_status st = norfx_erase_sector(ctx->dev, (uint16_t)block);
    return (st == NORFX_SUCCESS) ? LFS_ERR_OK : LFS_ERR_IO;
}

int norfx_lfs_sync(const struct lfs_config *c)
{
    /* NOR flash has no write cache — every program/erase completes before
     * the driver function returns. Nothing to flush. */
    (void)c;
    return LFS_ERR_OK;
}