/**
 * @file test_norfx_fast_read.c
 * @brief Tests for norfx_fast_read().
 */

#include "unity.h"
#include "nor_fx.h"
#include "port_linux.h"

#include <string.h>

static norfx_sim_ctx_t ctx;
static struct norfx_device dev;

void setUp(void)
{
    norfx_sim_init(&ctx, "/tmp/norfx_test_fast_read.bin", NORFX_SIM_FLASH_SIZE);
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

void test_fast_read_erased_returns_0xff(void)
{
    uint8_t buf[16];
    memset(buf, 0x00, sizeof(buf));

    enum norfx_status status = norfx_fast_read(&dev, 0, 0, sizeof(buf), buf);

    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        TEST_ASSERT_EQUAL_HEX8(0xFF, buf[i]);
    }
}

void test_fast_read_matches_read(void)
{
    /* Write known pattern directly into simulated flash. */
    uint8_t pattern[32];
    for (size_t i = 0; i < sizeof(pattern); i++)
    {
        pattern[i] = (uint8_t)(i * 3u);
    }
    memcpy(&ctx.flash[0], pattern, sizeof(pattern));

    uint8_t buf_read[32]      = {0};
    uint8_t buf_fast_read[32] = {0};

    norfx_read(&dev, 0, 0, sizeof(buf_read), buf_read);
    norfx_fast_read(&dev, 0, 0, sizeof(buf_fast_read), buf_fast_read);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(buf_read, buf_fast_read, sizeof(buf_read));
}

void test_fast_read_null_dev_returns_enodev(void)
{
    uint8_t buf[4];
    TEST_ASSERT_EQUAL(NORFX_ENODEV, norfx_fast_read(NULL, 0, 0, sizeof(buf), buf));
}

void test_fast_read_null_buf_returns_einval(void)
{
    TEST_ASSERT_EQUAL(NORFX_EINVAL, norfx_fast_read(&dev, 0, 0, 4, NULL));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fast_read_erased_returns_0xff);
    RUN_TEST(test_fast_read_matches_read);
    RUN_TEST(test_fast_read_null_dev_returns_enodev);
    RUN_TEST(test_fast_read_null_buf_returns_einval);

    return UNITY_END();
}
