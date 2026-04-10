/**
 * @file test_norfx_erase_sector.c
 * @brief Tests for norfx_erase_sector().
 */

#include "unity.h"
#include "nor_fx.h"
#include "port_linux.h"

#include <string.h>

static norfx_sim_ctx_t ctx;
static struct norfx_device dev;

void setUp(void)
{
    norfx_sim_init(&ctx, "/tmp/norfx_test_erase_sector.bin", NORFX_SIM_FLASH_SIZE);
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

void test_erase_sector_returns_success(void)
{
    enum norfx_status status = norfx_erase_sector(&dev, 0);
    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);
}

void test_erase_sector_fills_with_0xff(void)
{
    /* Dirty the sector directly. */
    memset(&ctx.flash[0], 0x00, NORFX_SIM_SECTOR_SIZE);

    norfx_erase_sector(&dev, 0);

    uint8_t buf[NORFX_SIM_SECTOR_SIZE];
    norfx_read(&dev, 0, 0, sizeof(buf), buf);

    for (size_t i = 0; i < sizeof(buf); i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }
}

void test_erase_sector_only_erases_target_sector(void)
{
    /* Dirty sector 0 and sector 1. */
    memset(&ctx.flash[0],                   0x00, NORFX_SIM_SECTOR_SIZE);
    memset(&ctx.flash[NORFX_SIM_SECTOR_SIZE], 0x00, NORFX_SIM_SECTOR_SIZE);

    /* Erase only sector 0. */
    norfx_erase_sector(&dev, 0);

    /* Sector 0 must be 0xFF. */
    uint8_t buf[4] = {0};
    norfx_read(&dev, 0, 0, sizeof(buf), buf);
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }

    /* Sector 1 must still be 0x00. */
    norfx_read(&dev, 16, 0, sizeof(buf), buf);  /* page 16 = sector 1 start */
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0x00, buf[i]);
    }
}

void test_erase_sector_null_dev_returns_enodev(void)
{
    TEST_ASSERT_EQUAL(NORFX_ENODEV, norfx_erase_sector(NULL, 0));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_erase_sector_returns_success);
    RUN_TEST(test_erase_sector_fills_with_0xff);
    RUN_TEST(test_erase_sector_only_erases_target_sector);
    RUN_TEST(test_erase_sector_null_dev_returns_enodev);

    return UNITY_END();
}
