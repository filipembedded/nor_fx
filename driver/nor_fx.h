/**
 * @file nor_fx.h
 * @brief Cross-platform NOR flash driver — public API.
 *
 * Provides a hardware-agnostic interface for SPI NOR flash devices.
 * All platform I/O is supplied via function pointers in @ref norfx_device,
 * making the driver portable across any MCU or OS.
 *
 * @note v1 supports single SPI mode only. QSPI/OSPI support is planned
 *       for a future revision.
 */

#ifndef NOR_FX_H
#define NOR_FX_H

#include <stdint.h>
#include <stddef.h>

/** @defgroup norfx_masks Status register bit masks
 *  @{
 */
/** Work-In-Progress bit in Status Register 1 (SR1[0]). */
#define NORFX_WIP_MASK                  0x01u
/** @} */

/** @defgroup norfx_geometry Flash geometry constants
 *
 * Physical parameters of the supported NOR flash device (W25Q128JV).
 * The LFS adapter layer uses these as the single source of truth when
 * translating between littlefs block/offset coordinates and driver
 * page/offset coordinates.
 *
 * @note These will be replaced by a runtime config struct in a future
 *       revision to support devices with different geometries.
 *  @{
 */
#define NORFX_PAGE_SIZE     256u        /**< Bytes per programmable page        */
#define NORFX_SECTOR_SIZE   4096u       /**< Bytes per erasable sector (4 KB)   */
#define NORFX_BLOCK_COUNT   4096u       /**< Total number of sectors (16 MB)    */
/** @} */

/** @defgroup norfx_timeouts Per-operation busy-wait timeout values (milliseconds)
 *
 * Derived from W25Q128JV datasheet worst-case timing.
 *  @{
 */
#define NORFX_RESET_TIMEOUT_MS          50u     /**< tRST max: 30 us, 50 ms margin    */
#define NORFX_WRITE_EN_TIMEOUT_MS       10u     /**< Write enable latch setup          */
#define NORFX_SECTOR_ERASE_TIMEOUT_MS   400u    /**< tSE  max: 400 ms                  */
#define NORFX_BLOCK_ERASE_TIMEOUT_MS    2000u   /**< tBE2 max: 2000 ms (64 KB block)   */
#define NORFX_PAGE_PROGRAM_TIMEOUT_MS   3u      /**< tPP  max: 3 ms                    */
/** @} */

/**
 * @brief Driver return codes.
 */
enum norfx_status {
    NORFX_SUCCESS = 0,  /**< Operation completed successfully.              */
    NORFX_ERROR   = 1,  /**< Generic, unrecoverable I/O error.              */
    NORFX_READY   = 2,  /**< Device is idle and ready to accept commands.   */
    NORFX_BUSY    = 3,  /**< Device is executing an internal operation.     */
    NORFX_TIMEOUT = 4,  /**< Operation did not complete within the timeout. */
    NORFX_ENODEV  = 5,  /**< Device handle (struct norfx_device *) is NULL. */
    NORFX_EINVAL  = 6,  /**< One or more arguments are invalid.             */
};

/**
 * @brief SPI instruction opcodes.
 *
 * Opcodes as defined in the W25Q128JV datasheet.
 * Only opcodes relevant to the SPI (single I/O) mode are listed.
 */
enum norfx_instruction {
    INST_WRITE_ENABLE             = 0x06,
    INST_VOLATILE_SR_WRITE_ENABLE = 0x50,
    INST_WRITE_DISABLE            = 0x04,

    INST_RELEASE_POWER_DOWN_ID    = 0xAB,
    INST_MANUFACTURER_DEVICE_ID   = 0x90,
    INST_JEDEC_ID                 = 0x9F,
    INST_READ_UNIQUE_ID           = 0x4B,

    INST_READ_DATA                = 0x03,
    INST_FAST_READ                = 0x0B,

    INST_PAGE_PROGRAM             = 0x02,

    INST_SECTOR_ERASE_4KB         = 0x20,
    INST_BLOCK_ERASE_32KB         = 0x52,
    INST_BLOCK_ERASE_64KB         = 0xD8,
    INST_CHIP_ERASE               = 0xC7,   /**< Alternate opcode: 0x60 */

    INST_READ_STATUS_REG_1        = 0x05,
    INST_WRITE_STATUS_REG_1       = 0x01,
    INST_READ_STATUS_REG_2        = 0x35,
    INST_WRITE_STATUS_REG_2       = 0x31,
    INST_READ_STATUS_REG_3        = 0x15,
    INST_WRITE_STATUS_REG_3       = 0x11,

    INST_READ_SFDP_REG            = 0x5A,
    INST_ERASE_SECURITY_REG       = 0x44,
    INST_PROGRAM_SECURITY_REG     = 0x42,
    INST_READ_SECURITY_REG        = 0x48,

    INST_GLOBAL_BLOCK_LOCK        = 0x7E,
    INST_GLOBAL_BLOCK_UNLOCK      = 0x98,
    INST_READ_BLOCK_LOCK          = 0x3D,
    INST_INDIVIDUAL_BLOCK_LOCK    = 0x36,
    INST_INDIVIDUAL_BLOCK_UNLOCK  = 0x39,

    INST_ERASE_PROGRAM_SUSPEND    = 0x75,
    INST_ERASE_PROGRAM_RESUME     = 0x7A,
    INST_POWER_DOWN               = 0xB9,

    INST_ENABLE_RESET             = 0x66,
    INST_RESET_DEVICE             = 0x99,
};

/**
 * @brief Selects which device ID to read via @ref norfx_read_id.
 */
enum norfx_id_kind {
    ID_RELEASE_POWER_DOWN  = 0, /**< Release Power-Down / Device ID (ABh)         */
    ID_MANUFACTURER_DEVICE = 1, /**< Manufacturer + Device ID (90h), 2 bytes       */
    ID_JEDEC               = 2, /**< JEDEC ID: MFR | MemType | Capacity (9Fh)      */
    ID_READ_UNIQUE         = 3, /**< 64-bit unique ID (4Bh), packed into 32 LSBs   */
};

/**
 * @brief Platform abstraction handle.
 *
 * The caller fills all function pointers before passing the struct to any
 * driver function. All callbacks receive the opaque @p context pointer so
 * the same driver instance can manage multiple physical devices.
 *
 * @note All callbacks must return @ref NORFX_SUCCESS on success or any
 *       other @ref norfx_status value on failure.
 */
struct norfx_device {
    void *context;  /**< Opaque pointer passed as-is to every callback.         */

    /** Assert chip-select (drive CS low). */
    enum norfx_status (*spi_chip_select)(void *context);

    /** De-assert chip-select (drive CS high). */
    enum norfx_status (*spi_chip_deselect)(void *context);

    /** Transmit @p size bytes from @p data over SPI. */
    enum norfx_status (*spi_write)(void *context, uint8_t *data, uint16_t size);

    /** Receive @p size bytes into @p data over SPI. */
    enum norfx_status (*spi_read)(void *context, uint8_t *data, uint16_t size);

    /** Return a free-running millisecond tick counter (wrapping is handled). */
    uint32_t (*get_tick_ms)(void *context);

    /** Block for at least @p delay milliseconds. */
    void (*delay_ms)(void *context, uint32_t delay);
};

/**
 * @brief Issue a software reset sequence (66h + 99h).
 *
 * Sends Enable Reset followed by Reset Device and then polls the WIP bit
 * until the device is ready or the timeout expires.
 *
 * @param[in] dev  Initialised device handle.
 * @return         @ref NORFX_SUCCESS, @ref NORFX_ENODEV, @ref NORFX_EINVAL,
 *                 @ref NORFX_ERROR, or @ref NORFX_TIMEOUT.
 */
enum norfx_status norfx_reset(struct norfx_device *dev);

/**
 * @brief Read one of the supported device identification values.
 *
 * @param[in]  dev     Initialised device handle.
 * @param[in]  id      Which ID type to retrieve (@ref norfx_id_kind).
 * @param[out] id_val  Receives the decoded identifier.
 * @return             @ref NORFX_SUCCESS, @ref NORFX_ENODEV, @ref NORFX_EINVAL,
 *                     or @ref NORFX_ERROR.
 */
enum norfx_status norfx_read_id(struct norfx_device *dev, enum norfx_id_kind id, uint32_t *id_val);

/**
 * @brief Read @p size bytes starting at (@p start_page, @p offset).
 *
 * Uses the standard Read Data instruction (03h, max frequency limited by
 * device). For higher-speed reads use @ref norfx_fast_read.
 *
 * @param[in]  dev         Initialised device handle.
 * @param[in]  start_page  Zero-based page number (256 bytes per page).
 * @param[in]  offset      Byte offset within the starting page (0–255).
 * @param[in]  size        Number of bytes to read.
 * @param[out] rx_buf      Caller-allocated buffer of at least @p size bytes.
 * @return                 @ref NORFX_SUCCESS, @ref NORFX_ENODEV,
 *                         @ref NORFX_EINVAL, or @ref NORFX_ERROR.
 */
enum norfx_status norfx_read(struct norfx_device *dev,
                             uint32_t start_page,
                             uint8_t  offset,
                             uint32_t size,
                             uint8_t *rx_buf);

/**
 * @brief Read @p size bytes using the Fast Read instruction (0Bh).
 *
 * Identical to @ref norfx_read but inserts one dummy byte after the
 * address to allow higher SPI clock frequencies.
 *
 * @param[in]  dev         Initialised device handle.
 * @param[in]  start_page  Zero-based page number.
 * @param[in]  offset      Byte offset within the starting page (0–255).
 * @param[in]  size        Number of bytes to read.
 * @param[out] rx_buf      Caller-allocated buffer of at least @p size bytes.
 * @return                 @ref NORFX_SUCCESS, @ref NORFX_ENODEV,
 *                         @ref NORFX_EINVAL, or @ref NORFX_ERROR.
 */
enum norfx_status norfx_fast_read(struct norfx_device *dev,
                                  uint32_t start_page,
                                  uint8_t  offset,
                                  uint32_t size,
                                  uint8_t *rx_buf);

/**
 * @brief Set the Write Enable Latch (WEL) on the device.
 *
 * Must be called before any erase or program operation. The latch is
 * automatically cleared by the device after a successful operation.
 *
 * @param[in] dev  Initialised device handle.
 * @return         @ref NORFX_SUCCESS, @ref NORFX_ENODEV, @ref NORFX_EINVAL,
 *                 or @ref NORFX_ERROR.
 */
enum norfx_status norfx_write_enable(struct norfx_device *dev);

/**
 * @brief Clear the Write Enable Latch (WEL) on the device.
 *
 * @param[in] dev  Initialised device handle.
 * @return         @ref NORFX_SUCCESS, @ref NORFX_ENODEV, @ref NORFX_EINVAL,
 *                 or @ref NORFX_ERROR.
 */
enum norfx_status norfx_write_disable(struct norfx_device *dev);

/**
 * @brief Erase a 4 KB sector.
 *
 * Sets WEL, issues Sector Erase (20h), polls WIP until the erase
 * completes or @ref NORFX_SECTOR_ERASE_TIMEOUT_MS elapses, then
 * clears WEL. On any failure after WEL is set, a best-effort
 * Write Disable is issued to leave the device in a safe state.
 *
 * @param[in] dev         Initialised device handle.
 * @param[in] num_sector  Zero-based sector index to erase.
 * @return                @ref NORFX_SUCCESS, @ref NORFX_ENODEV,
 *                        @ref NORFX_EINVAL, @ref NORFX_ERROR,
 *                        or @ref NORFX_TIMEOUT.
 */
enum norfx_status norfx_erase_sector(struct norfx_device *dev, uint16_t num_sector);

/**
 * @brief Read Status Register 1.
 *
 * @param[in]  dev         Initialised device handle.
 * @param[out] status_reg  Receives the raw value of Status Register 1.
 * @return                 @ref NORFX_SUCCESS, @ref NORFX_ENODEV,
 *                         @ref NORFX_EINVAL, or @ref NORFX_ERROR.
 */
enum norfx_status norfx_read_status_reg(struct norfx_device *dev, uint8_t *status_reg);

/**
 * @brief Program up to 256 bytes into a single page.
 *
 * Sets WEL, sends the Page Program instruction (02h) followed by the
 * 24-bit address and the data payload as two back-to-back SPI writes
 * (zero-copy — no intermediate buffer), then polls WIP until done.
 *
 * @note The caller is responsible for ensuring the target page has been
 *       erased before calling this function. Writing to a non-erased page
 *       results in undefined flash content.
 *
 * @param[in] dev     Initialised device handle.
 * @param[in] page    Zero-based page number.
 * @param[in] offset  Byte offset within the page (0–255).
 * @param[in] size    Number of bytes to program (1 – (256 - offset)).
 * @param[in] data    Pointer to the data buffer to program.
 * @return            @ref NORFX_SUCCESS, @ref NORFX_ENODEV,
 *                    @ref NORFX_EINVAL, @ref NORFX_ERROR,
 *                    or @ref NORFX_TIMEOUT.
 */
enum norfx_status norfx_page_program(struct norfx_device *dev,
                                     uint32_t page,
                                     uint16_t offset,
                                     uint32_t size,
                                     uint8_t *data);

/**
 * @brief Write arbitrary data to flash with automatic read-modify-write.
 *
 * Handles writes that span multiple sectors by performing a
 * read-modify-write cycle for each affected sector:
 *   1. Read the full sector (4096 B) into @p scratch_buf.
 *   2. Overlay the new data at the correct offset.
 *   3. Erase the sector.
 *   4. Re-program all 16 pages from @p scratch_buf.
 *
 * @note Do NOT use this function with a file system such as littlefs.
 *       littlefs manages erase/program ordering itself and expects
 *       @ref norfx_page_program and @ref norfx_erase_sector to be called
 *       directly without implicit erase.
 *
 * @param[in]  dev         Initialised device handle.
 * @param[in]  page        Zero-based starting page number.
 * @param[in]  offset      Byte offset within the starting page (0–255).
 * @param[in]  size        Total number of bytes to write.
 * @param[in]  data        Source data buffer.
 * @param[out] scratch_buf Caller-provided buffer of exactly 4096 bytes used
 *                         as a temporary sector workspace. Must not be NULL.
 *                         Avoids placing a 4 KB array on the stack.
 * @return                 @ref NORFX_SUCCESS, @ref NORFX_ENODEV,
 *                         @ref NORFX_EINVAL, @ref NORFX_ERROR,
 *                         or @ref NORFX_TIMEOUT.
 */
enum norfx_status norfx_write(struct norfx_device *dev,
                              uint32_t page,
                              uint16_t offset,
                              uint32_t size,
                              uint8_t *data,
                              uint8_t *scratch_buf);

#endif // NOR_FX_H
