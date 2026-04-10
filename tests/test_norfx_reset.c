/**
 * @file test_norfx_reset.c
 * @brief Tests for norfx_reset().
 */

#include "unity.h"
#include "nor_fx.h"
#include "port_linux.h"

static norfx_sim_ctx_t ctx;
static struct norfx_device dev;

void setUp(void)
{
    norfx_sim_init(&ctx, "/tmp/norfx_test_reset.bin", NORFX_SIM_FLASH_SIZE);
    norfx_sim_chip_erase(&ctx);

    dev.context          = &ctx;
    dev.spi_chip_select  = norfx_sim_cs_select;
    dev.spi_chip_deselect = norfx_sim_cs_deselect;
    dev.spi_write        = norfx_sim_spi_write;
    dev.spi_read         = norfx_sim_spi_read;
    dev.get_tick_ms      = norfx_sim_get_tick_ms;
    dev.delay_ms         = norfx_sim_delay_ms;
}

void tearDown(void)
{
    norfx_sim_deinit(&ctx);
}

void test_reset_returns_success(void)
{
    enum norfx_status status = norfx_reset(&dev);
    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);
}

void test_reset_null_dev_returns_enodev(void)
{
    enum norfx_status status = norfx_reset(NULL);
    TEST_ASSERT_EQUAL(NORFX_ENODEV, status);
}

void test_reset_null_callback_returns_einval(void)
{
    struct norfx_device bad_dev = dev;
    bad_dev.spi_write = NULL;

    enum norfx_status status = norfx_reset(&bad_dev);
    TEST_ASSERT_EQUAL(NORFX_EINVAL, status);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_reset_returns_success);
    RUN_TEST(test_reset_null_dev_returns_enodev);
    RUN_TEST(test_reset_null_callback_returns_einval);

    return UNITY_END();
}
