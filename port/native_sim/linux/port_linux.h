/**
 * @file port_linux.h
 * @brief Linux simulation port for the nor-flash-xplat driver.
 *
 * Provides a fully in-process NOR flash simulator backed by a plain file
 * on the host filesystem. No real SPI hardware is involved.
 *
 * Flash behaviour modelled:
 *  - Reads always succeed (return 0xFF for erased, programmed value otherwise).
 *  - Page program can only clear bits (0 → 1 is not possible without erase).
 *  - Sector erase (4 KB) sets all bytes in the sector to 0xFF.
 *  - Busy-wait (WIP) is simulated with a short configurable delay.
 *
 * Typical usage:
 * @code
 *   norfx_sim_ctx_t ctx;
 *   norfx_sim_init(&ctx, "flash.bin", NORFX_SIM_FLASH_SIZE);
 *
 *   struct norfx_device dev = {
 *       .context          = &ctx,
 *       .spi_chip_select   = norfx_sim_cs_select,
 *       .spi_chip_deselect = norfx_sim_cs_deselect,
 *       .spi_write         = norfx_sim_spi_write,
 *       .spi_read          = norfx_sim_spi_read,
 *       .get_tick_ms       = norfx_sim_get_tick_ms,
 *       .delay_ms          = norfx_sim_delay_ms,
 *   };
 *
 *   norfx_reset(&dev);
 *   // ... use driver normally ...
 *   norfx_sim_deinit(&ctx);
 * @endcode
 */

#ifndef PORT_LINUX_H
#define PORT_LINUX_H

#include "nor_fx.h"
#include <stdint.h>
#include <stddef.h>

/** Total simulated flash size in bytes (16 MB — matches W25Q128). */
#define NORFX_SIM_FLASH_SIZE    (16u * 1024u * 1024u)

/** Sector size in bytes (4 KB). */
#define NORFX_SIM_SECTOR_SIZE   (4096u)

/** Page size in bytes (256 B). */
#define NORFX_SIM_PAGE_SIZE     (256u)

/**
 * @brief Internal state machine for decoding the SPI command stream.
 */
typedef enum {
    SIM_CMD_IDLE = 0,       /**< No active command.                          */
    SIM_CMD_READ,           /**< Standard Read Data (03h).                   */
    SIM_CMD_FAST_READ,      /**< Fast Read (0Bh) — dummy byte consumed.      */
    SIM_CMD_PAGE_PROGRAM,   /**< Page Program (02h).                         */
    SIM_CMD_SECTOR_ERASE,   /**< Sector Erase (20h).                         */
    SIM_CMD_READ_STATUS,    /**< Read Status Register 1 (05h).               */
    SIM_CMD_WRITE_ENABLE,   /**< Write Enable (06h).                         */
    SIM_CMD_WRITE_DISABLE,  /**< Write Disable (04h).                        */
    SIM_CMD_ENABLE_RESET,   /**< Enable Reset (66h) — awaiting 99h.         */
    SIM_CMD_READ_JEDEC_ID,  /**< JEDEC ID (9Fh).                             */
} norfx_sim_cmd_t;

/**
 * @brief Simulation context.
 *
 * One instance per simulated device. Must be initialised with
 * @ref norfx_sim_init before use and released with @ref norfx_sim_deinit.
 */
typedef struct {
    uint8_t  *flash;            /**< Memory-mapped flash image.              */
    size_t    flash_size;       /**< Total size of the flash image in bytes.  */
    int       fd;               /**< File descriptor for the backing file.    */
    uint8_t   status_reg;       /**< Simulated Status Register 1.            */
    int       cs_active;        /**< Non-zero when chip-select is asserted.   */

    /* SPI command decoder state */
    norfx_sim_cmd_t cmd;        /**< Currently active command.               */
    uint8_t  addr_buf[4];       /**< Address accumulator.                    */
    uint8_t  addr_bytes_rx;     /**< Number of address bytes received so far. */
    uint32_t current_addr;      /**< Decoded 24-bit flash address.           */
    uint8_t  dummy_bytes_rx;    /**< Dummy bytes consumed for Fast Read.     */
} norfx_sim_ctx_t;

/**
 * @brief Initialise the simulation context and backing file.
 *
 * Creates @p path if it does not exist and maps it into memory. If the file
 * is new it is filled with 0xFF (erased state). An existing file retains its
 * content, allowing test state to persist across runs.
 *
 * @param[out] ctx         Context to initialise.
 * @param[in]  path        Path to the backing file (created if absent).
 * @param[in]  flash_size  Size of the simulated flash in bytes.
 * @return                 0 on success, -1 on error (errno set).
 */
int norfx_sim_init(norfx_sim_ctx_t *ctx, const char *path, size_t flash_size);

/**
 * @brief Flush and release all resources held by the context.
 *
 * Syncs the memory map to disk and closes the file descriptor.
 *
 * @param[in] ctx  Context to release.
 */
void norfx_sim_deinit(norfx_sim_ctx_t *ctx);

/**
 * @brief Erase the entire simulated flash to 0xFF.
 *
 * Convenience helper for test setup — equivalent to a chip erase.
 *
 * @param[in] ctx  Initialised simulation context.
 */
void norfx_sim_chip_erase(norfx_sim_ctx_t *ctx);

/* ---------------------------------------------------------------------------
 * norfx_device callbacks — pass these directly to struct norfx_device
 * ------------------------------------------------------------------------- */

/** @ref norfx_device::spi_chip_select callback. */
enum norfx_status norfx_sim_cs_select(void *context);

/** @ref norfx_device::spi_chip_deselect callback. */
enum norfx_status norfx_sim_cs_deselect(void *context);

/** @ref norfx_device::spi_write callback. */
enum norfx_status norfx_sim_spi_write(void *context, uint8_t *data, uint16_t size);

/** @ref norfx_device::spi_read callback. */
enum norfx_status norfx_sim_spi_read(void *context, uint8_t *data, uint16_t size);

/** @ref norfx_device::get_tick_ms callback. */
uint32_t norfx_sim_get_tick_ms(void *context);

/** @ref norfx_device::delay_ms callback. */
void norfx_sim_delay_ms(void *context, uint32_t delay);

#endif /* PORT_LINUX_H */