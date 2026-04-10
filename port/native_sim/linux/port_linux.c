/**
 * @file port_linux.c
 * @brief Linux simulation port for the nor-flash-xplat driver.
 *
 * Implements a NOR flash simulator backed by a memory-mapped file.
 * The SPI command stream is decoded in software to replicate real
 * W25Q128 behaviour without any hardware.
 *
 * Simulated behaviour:
 *  - WEL / WIP bits in the status register.
 *  - Sector erase: fills 4 KB with 0xFF, sets WIP for the erase duration.
 *  - Page program: ANDs incoming bytes with existing flash content
 *    (bits can only be cleared, not set — correct NOR behaviour).
 *  - Read / Fast Read: returns flash content directly.
 *  - JEDEC ID: returns Winbond W25Q128JV values (EF 70 18).
 */

#include "port_linux.h"
#include "nor_fx.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * Internal constants
 * ------------------------------------------------------------------------- */

/** WIP bit in Status Register 1. */
#define SR1_WIP_BIT     0x01u

/** WEL bit in Status Register 1. */
#define SR1_WEL_BIT     0x02u

/** JEDEC ID bytes for Winbond W25Q128JV: Manufacturer | MemType | Capacity */
#define JEDEC_MFR       0xEFu
#define JEDEC_MEM_TYPE  0x70u
#define JEDEC_CAPACITY  0x18u

/** Number of address bytes in a 24-bit NOR flash address. */
#define ADDR_BYTES      3u

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/**
 * @brief Decode three address bytes accumulated in ctx->addr_buf into a
 *        single 32-bit flash address stored in ctx->current_addr.
 */
static void decode_address(norfx_sim_ctx_t *ctx)
{
    ctx->current_addr = ((uint32_t)ctx->addr_buf[0] << 16u) |
                        ((uint32_t)ctx->addr_buf[1] <<  8u) |
                        ((uint32_t)ctx->addr_buf[2]);
}

/**
 * @brief Return non-zero if @p addr is within the mapped flash region.
 */
static int addr_valid(const norfx_sim_ctx_t *ctx, uint32_t addr)
{
    return (addr < ctx->flash_size);
}

/* ---------------------------------------------------------------------------
 * Init / deinit
 * ------------------------------------------------------------------------- */

int norfx_sim_init(norfx_sim_ctx_t *ctx, const char *path, size_t flash_size)
{
    if (ctx == NULL || path == NULL || flash_size == 0u)
    {
        errno = EINVAL;
        return -1;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->flash_size = flash_size;

    /* Open (or create) the backing file. */
    ctx->fd = open(path, O_RDWR | O_CREAT, 0600);
    if (ctx->fd < 0)
    {
        perror("norfx_sim_init: open");
        return -1;
    }

    /* Extend to the requested size if the file is smaller. */
    struct stat st;
    if (fstat(ctx->fd, &st) != 0)
    {
        perror("norfx_sim_init: fstat");
        close(ctx->fd);
        return -1;
    }

    if ((size_t)st.st_size < flash_size)
    {
        /* Fill new space with 0xFF (erased NOR flash state). */
        if (ftruncate(ctx->fd, (off_t)flash_size) != 0)
        {
            perror("norfx_sim_init: ftruncate");
            close(ctx->fd);
            return -1;
        }

        /* Write 0xFF over the new region. */
        uint8_t ff = 0xFFu;
        for (size_t i = (size_t)st.st_size; i < flash_size; i++)
        {
            if (pwrite(ctx->fd, &ff, 1, (off_t)i) != 1)
            {
                perror("norfx_sim_init: pwrite");
                close(ctx->fd);
                return -1;
            }
        }
    }

    /* Memory-map the file for zero-copy read/write access. */
    ctx->flash = mmap(NULL, flash_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, ctx->fd, 0);
    if (ctx->flash == MAP_FAILED)
    {
        perror("norfx_sim_init: mmap");
        close(ctx->fd);
        return -1;
    }

    ctx->status_reg   = 0x00u;
    ctx->cs_active    = 0;
    ctx->cmd          = SIM_CMD_IDLE;

    return 0;
}

void norfx_sim_deinit(norfx_sim_ctx_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    if (ctx->flash != NULL && ctx->flash != MAP_FAILED)
    {
        msync(ctx->flash, ctx->flash_size, MS_SYNC);
        munmap(ctx->flash, ctx->flash_size);
        ctx->flash = NULL;
    }

    if (ctx->fd >= 0)
    {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

void norfx_sim_chip_erase(norfx_sim_ctx_t *ctx)
{
    if (ctx == NULL || ctx->flash == NULL)
    {
        return;
    }

    memset(ctx->flash, 0xFF, ctx->flash_size);
}

/* ---------------------------------------------------------------------------
 * Chip-select callbacks
 * ------------------------------------------------------------------------- */

enum norfx_status norfx_sim_cs_select(void *context)
{
    norfx_sim_ctx_t *ctx = (norfx_sim_ctx_t *)context;

    if (ctx == NULL)
    {
        return NORFX_ENODEV;
    }

    ctx->cs_active        = 1;
    ctx->cmd              = SIM_CMD_IDLE;
    ctx->addr_bytes_rx    = 0u;
    ctx->dummy_bytes_rx   = 0u;
    ctx->current_addr     = 0u;

    return NORFX_SUCCESS;
}

enum norfx_status norfx_sim_cs_deselect(void *context)
{
    norfx_sim_ctx_t *ctx = (norfx_sim_ctx_t *)context;

    if (ctx == NULL)
    {
        return NORFX_ENODEV;
    }

    /* Commit any pending operation when CS is released. */
    if (ctx->cmd == SIM_CMD_SECTOR_ERASE && ctx->addr_bytes_rx == ADDR_BYTES)
    {
        decode_address(ctx);
        uint32_t sector_base = ctx->current_addr & ~(NORFX_SIM_SECTOR_SIZE - 1u);

        if (addr_valid(ctx, sector_base))
        {
            memset(&ctx->flash[sector_base], 0xFF, NORFX_SIM_SECTOR_SIZE);
        }

        /* Clear WIP + WEL after erase completes. */
        ctx->status_reg &= (uint8_t)~(SR1_WIP_BIT | SR1_WEL_BIT);
    }

    if (ctx->cmd == SIM_CMD_PAGE_PROGRAM)
    {
        /* WIP + WEL cleared after program completes. */
        ctx->status_reg &= (uint8_t)~(SR1_WIP_BIT | SR1_WEL_BIT);
    }

    ctx->cs_active      = 0;
    ctx->cmd            = SIM_CMD_IDLE;
    ctx->addr_bytes_rx  = 0u;
    ctx->dummy_bytes_rx = 0u;

    return NORFX_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * SPI write callback — decodes the command stream
 * ------------------------------------------------------------------------- */

enum norfx_status norfx_sim_spi_write(void *context, uint8_t *data, uint16_t size)
{
    norfx_sim_ctx_t *ctx = (norfx_sim_ctx_t *)context;

    if (ctx == NULL || data == NULL)
    {
        return NORFX_ENODEV;
    }

    if (!ctx->cs_active || size == 0u)
    {
        return NORFX_ERROR;
    }

    uint16_t idx = 0u;

    /* -----------------------------------------------------------------
     * First byte is the opcode when the command decoder is idle.
     * ----------------------------------------------------------------- */
    if (ctx->cmd == SIM_CMD_IDLE)
    {
        uint8_t opcode = data[idx++];

        switch (opcode)
        {
            case INST_READ_DATA:
                ctx->cmd           = SIM_CMD_READ;
                ctx->addr_bytes_rx = 0u;
                break;

            case INST_FAST_READ:
                ctx->cmd           = SIM_CMD_FAST_READ;
                ctx->addr_bytes_rx = 0u;
                ctx->dummy_bytes_rx = 0u;
                break;

            case INST_PAGE_PROGRAM:
                ctx->cmd           = SIM_CMD_PAGE_PROGRAM;
                ctx->addr_bytes_rx = 0u;
                /* Set WIP — program in progress. */
                ctx->status_reg |= SR1_WIP_BIT;
                break;

            case INST_SECTOR_ERASE_4KB:
                ctx->cmd           = SIM_CMD_SECTOR_ERASE;
                ctx->addr_bytes_rx = 0u;
                /* Set WIP — erase in progress. */
                ctx->status_reg |= SR1_WIP_BIT;
                break;

            case INST_READ_STATUS_REG_1:
                ctx->cmd = SIM_CMD_READ_STATUS;
                break;

            case INST_WRITE_ENABLE:
                ctx->status_reg |= SR1_WEL_BIT;
                ctx->cmd = SIM_CMD_IDLE;
                break;

            case INST_WRITE_DISABLE:
                ctx->status_reg &= (uint8_t)~SR1_WEL_BIT;
                ctx->cmd = SIM_CMD_IDLE;
                break;

            case INST_ENABLE_RESET:
                ctx->cmd = SIM_CMD_ENABLE_RESET;
                break;

            case INST_RESET_DEVICE:
                if (ctx->cmd == SIM_CMD_ENABLE_RESET)
                {
                    /* Soft reset — clear status register. */
                    ctx->status_reg = 0x00u;
                }
                ctx->cmd = SIM_CMD_IDLE;
                break;

            case INST_JEDEC_ID:
                ctx->cmd = SIM_CMD_READ_JEDEC_ID;
                break;

            default:
                /* Unknown opcode — ignore. */
                ctx->cmd = SIM_CMD_IDLE;
                break;
        }
    }

    /* -----------------------------------------------------------------
     * Consume remaining bytes in the write buffer according to the
     * active command (address bytes, then data payload).
     * ----------------------------------------------------------------- */
    while (idx < size)
    {
        uint8_t byte = data[idx++];

        switch (ctx->cmd)
        {
            case SIM_CMD_READ:
            case SIM_CMD_FAST_READ:
            case SIM_CMD_PAGE_PROGRAM:
            case SIM_CMD_SECTOR_ERASE:
                /* Accumulate 3-byte address. */
                if (ctx->addr_bytes_rx < ADDR_BYTES)
                {
                    ctx->addr_buf[ctx->addr_bytes_rx++] = byte;

                    if (ctx->addr_bytes_rx == ADDR_BYTES)
                    {
                        decode_address(ctx);
                    }
                }
                else if (ctx->cmd == SIM_CMD_FAST_READ &&
                         ctx->dummy_bytes_rx == 0u)
                {
                    /* Consume the one mandatory dummy byte. */
                    ctx->dummy_bytes_rx = 1u;
                }
                else if (ctx->cmd == SIM_CMD_PAGE_PROGRAM &&
                         ctx->addr_bytes_rx == ADDR_BYTES)
                {
                    /* Data payload — AND with existing content (NOR behaviour). */
                    if (addr_valid(ctx, ctx->current_addr))
                    {
                        ctx->flash[ctx->current_addr] &= byte;
                        ctx->current_addr++;
                    }
                }
                break;

            default:
                break;
        }
    }

    return NORFX_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * SPI read callback — returns data according to the active command
 * ------------------------------------------------------------------------- */

enum norfx_status norfx_sim_spi_read(void *context, uint8_t *data, uint16_t size)
{
    norfx_sim_ctx_t *ctx = (norfx_sim_ctx_t *)context;

    if (ctx == NULL || data == NULL)
    {
        return NORFX_ENODEV;
    }

    if (!ctx->cs_active || size == 0u)
    {
        return NORFX_ERROR;
    }

    switch (ctx->cmd)
    {
        case SIM_CMD_READ:
        case SIM_CMD_FAST_READ:
            for (uint16_t i = 0u; i < size; i++)
            {
                if (addr_valid(ctx, ctx->current_addr))
                {
                    data[i] = ctx->flash[ctx->current_addr++];
                }
                else
                {
                    data[i] = 0xFFu;
                }
            }
            break;

        case SIM_CMD_READ_STATUS:
            data[0] = ctx->status_reg;
            break;

        case SIM_CMD_READ_JEDEC_ID:
            if (size >= 3u)
            {
                data[0] = JEDEC_MFR;
                data[1] = JEDEC_MEM_TYPE;
                data[2] = JEDEC_CAPACITY;
            }
            break;

        default:
            memset(data, 0xFFu, size);
            break;
    }

    return NORFX_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * Tick and delay callbacks
 * ------------------------------------------------------------------------- */

uint32_t norfx_sim_get_tick_ms(void *context)
{
    (void)context;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint32_t)((ts.tv_sec * 1000u) + (ts.tv_nsec / 1000000u));
}

void norfx_sim_delay_ms(void *context, uint32_t delay)
{
    (void)context;

    struct timespec ts = {
        .tv_sec  = (time_t)(delay / 1000u),
        .tv_nsec = (long)((delay % 1000u) * 1000000u),
    };

    nanosleep(&ts, NULL);
}
