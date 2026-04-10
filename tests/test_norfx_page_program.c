/**
 * @file test_norfx_page_program.c
 * @brief Tests for norfx_page_program().
 */

#include "unity.h"
#include "nor_fx.h"
#include "port_linux.h"

#include <string.h>

static norfx_sim_ctx_t ctx;
static struct norfx_device dev;

void setUp(void)
{
    norfx_sim_init(&ctx, "/tmp/norfx_test_page_program.bin", NORFX_SIM_FLASH_SIZE);
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

void test_page_program_writes_data(void)
{
    uint8_t tx[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    uint8_t rx[8] = {0};

    enum norfx_status status = norfx_page_program(&dev, 0, 0, sizeof(tx), tx);
    TEST_ASSERT_EQUAL(NORFX_SUCCESS, status);

    norfx_read(&dev, 0, 0, sizeof(rx), rx);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));
}

void test_page_program_nor_and_behaviour(void)
{
    /* Pre-program with 0xF0 — only upper nibble set. */
    uint8_t first[4]  = {0xF0, 0xF0, 0xF0, 0xF0};
    uint8_t second[4] = {0x0F, 0x0F, 0x0F, 0x0F};
    /* Expected: 0xF0 AND 0x0F = 0x00 — bits can only be cleared. */
    uint8_t expected[4] = {0x00, 0x00, 0x00, 0x00};
    uint8_t rx[4] = {0};

    norfx_page_program(&dev, 0, 0, sizeof(first), first);
    norfx_page_program(&dev, 0, 0, sizeof(second), second);

    norfx_read(&dev, 0, 0, sizeof(rx), rx);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, rx, sizeof(expected));
}

void test_page_program_with_offset(void)
{
    uint8_t tx[4]  = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t rx[4]  = {0};

    /* Write at offset 16 within page 0. */
    norfx_page_program(&dev, 0, 16, sizeof(tx), tx);
    norfx_read(&dev, 0, 16, sizeof(rx), rx);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(tx, rx, sizeof(tx));
}

void test_page_program_null_dev_returns_enodev(void)
{
    uint8_t tx[4] = {0};
    TEST_ASSERT_EQUAL(NORFX_ENODEV, norfx_page_program(NULL, 0, 0, sizeof(tx), tx));
}

void test_page_program_null_data_returns_einval(void)
{
    TEST_ASSERT_EQUAL(NORFX_EINVAL, norfx_page_program(&dev, 0, 0, 4, NULL));
}

void test_page_program_size_zero_returns_einval(void)
{
    uint8_t tx[4] = {0};
    TEST_ASSERT_EQUAL(NORFX_EINVAL, norfx_page_program(&dev, 0, 0, 0, tx));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_page_program_writes_data);
    RUN_TEST(test_page_program_nor_and_behaviour);
    RUN_TEST(test_page_program_with_offset);
    RUN_TEST(test_page_program_null_dev_returns_enodev);
    RUN_TEST(test_page_program_null_data_returns_einval);
    RUN_TEST(test_page_program_size_zero_returns_einval);

    return UNITY_END();
}
