/**
 * @file test_norfx_read.c
 * @brief Tests for norfx_read().
 */

#include "unity.h"
#include "nor_fx.h"
#include "port_linux.h"

#include <string.h>

static norfx_sim_ctx_t ctx;
static struct norfx_device dev;

void setUp(void)
{
    norfx_sim_init(&ctx, "/tmp/norfx_test_read.bin", NORFX_SIM_FLASH_SIZE);
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

void test_read_erased_page_returns_0xff(void)
{
    uint8_t buf[16];
    memset(buf, 0x00, sizeof(buf));

    enum norfx_status status = norfx_read(&dev, 0, 0, sizeof(buf), buf);

    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }
}

void test_read_null_dev_returns_enodev(void)
{
    uint8_t buf[4];
    TEST_ASSERT_EQUAL(NORFX_ENODEV, norfx_read(NULL, 0, 0, sizeof(buf), buf));
}

void test_read_null_buf_returns_einval(void)
{
    TEST_ASSERT_EQUAL(NORFX_EINVAL, norfx_read(&dev, 0, 0, 4, NULL));
}

void test_read_programmed_data(void)
{
    /* Directly write known data into the simulated flash. */
    uint8_t expected[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    memcpy(&ctx.flash[0], expected, sizeof(expected));

    uint8_t buf[8] = {0};
    enum norfx_status status = norfx_read(&dev, 0, 0, sizeof(buf), buf);

    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(expected));
}

void test_read_with_page_offset(void)
{
    /* Write at page 1, offset 0. */
    uint8_t expected[4] = {0xAA, 0xBB, 0xCC, 0xDD};
    memcpy(&ctx.flash[256], expected, sizeof(expected));

    uint8_t buf[4] = {0};
    enum norfx_status status = norfx_read(&dev, 1, 0, sizeof(buf), buf);

    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(expected));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_read_erased_page_returns_0xff);
    RUN_TEST(test_read_null_dev_returns_enodev);
    RUN_TEST(test_read_null_buf_returns_einval);
    RUN_TEST(test_read_programmed_data);
    RUN_TEST(test_read_with_page_offset);

    return UNITY_END();
}
