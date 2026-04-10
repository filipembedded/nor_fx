/**
 * @file test_norfx_write.c
 * @brief Tests for norfx_write() — read-modify-write high-level API.
 */

#include "unity.h"
#include "nor_fx.h"
#include "port_linux.h"

#include <string.h>

#define SECTOR_SIZE 4096u

static norfx_sim_ctx_t ctx;
static struct norfx_device dev;
static uint8_t scratch[SECTOR_SIZE];

void setUp(void)
{
    norfx_sim_init(&ctx, "/tmp/norfx_test_write.bin", NORFX_SIM_FLASH_SIZE);
    norfx_sim_chip_erase(&ctx);

    dev.context           = &ctx;
    dev.spi_chip_select   = norfx_sim_cs_select;
    dev.spi_chip_deselect = norfx_sim_cs_deselect;
    dev.spi_write         = norfx_sim_spi_write;
    dev.spi_read          = norfx_sim_spi_read;
    dev.get_tick_ms       = norfx_sim_get_tick_ms;
    dev.delay_ms          = norfx_sim_delay_ms;
}

void tearDown(void)
{
    norfx_sim_deinit(&ctx);
}

void test_write_single_page(void)
{
    uint8_t tx[16] = {0x01, 0x02, 0x03, 0x04,
                      0x05, 0x06, 0x07, 0x08,
                      0x09, 0x0A, 0x0B, 0x0C,
                      0x0D, 0x0E, 0x0F, 0x10};
    uint8_t rx[16] = {0};

    enum norfx_status status = norfx_write(&dev, 0, 0, sizeof(tx), tx, scratch);
    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);

    norfx_read(&dev, 0, 0, sizeof(rx), rx);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));
}

void test_write_preserves_surrounding_data(void)
{
    /* Pre-populate full sector with 0xAB via page_program. */
    uint8_t fill[256];
    memset(fill, 0xAB, sizeof(fill));
    for (uint32_t p = 0; p < 16u; p++)
    {
        norfx_page_program(&dev, p, 0, sizeof(fill), fill);
    }

    /* Overwrite only 4 bytes at page 0, offset 10. */
    uint8_t patch[4] = {0x11, 0x22, 0x33, 0x44};
    norfx_write(&dev, 0, 10, sizeof(patch), patch, scratch);

    /* Patched bytes must match. */
    uint8_t rx[4] = {0};
    norfx_read(&dev, 0, 10, sizeof(rx), rx);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(patch, rx, sizeof(patch));

    /* Byte just before the patch must be untouched. */
    uint8_t before = 0x00;
    norfx_read(&dev, 0, 9, 1, &before);
    TEST_ASSERT_EQUAL_HEX8(0xAB, before);

    /* Byte just after the patch must be untouched. */
    uint8_t after = 0x00;
    norfx_read(&dev, 0, 14, 1, &after);
    TEST_ASSERT_EQUAL_HEX8(0xAB, after);
}

void test_write_spanning_two_sectors(void)
{
    /* Write data that crosses the boundary between sector 0 and sector 1.
     * Sector 0: pages 0-15, sector 1: pages 16-31.
     * Place the write so it ends in sector 1. */
    uint8_t tx[8]  = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    uint8_t rx[8]  = {0};

    /* Page 15 is the last page of sector 0, offset 252 puts us 4 bytes before
     * the sector boundary — the remaining 4 bytes spill into sector 1. */
    enum norfx_status status = norfx_write(&dev, 15, 252, sizeof(tx), tx, scratch);
    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);

    norfx_read(&dev, 15, 252, sizeof(rx), rx);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));
}

void test_write_null_dev_returns_enodev(void)
{
    uint8_t tx[4] = {0};
    TEST_ASSERT_EQUAL(NORFX_ENODEV, norfx_write(NULL, 0, 0, sizeof(tx), tx, scratch));
}

void test_write_null_data_returns_einval(void)
{
    TEST_ASSERT_EQUAL(NORFX_EINVAL, norfx_write(&dev, 0, 0, 4, NULL, scratch));
}

void test_write_null_scratch_returns_einval(void)
{
    uint8_t tx[4] = {0};
    TEST_ASSERT_EQUAL(NORFX_EINVAL, norfx_write(&dev, 0, 0, sizeof(tx), tx, NULL));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_write_single_page);
    RUN_TEST(test_write_preserves_surrounding_data);
    RUN_TEST(test_write_spanning_two_sectors);
    RUN_TEST(test_write_null_dev_returns_enodev);
    RUN_TEST(test_write_null_data_returns_einval);
    RUN_TEST(test_write_null_scratch_returns_einval);

    return UNITY_END();
}
