/**
 * @file test_norfx_lfs.c
 * @brief Integration tests for the nor_fx ↔ littlefs adapter layer.
 *
 * Each test mounts a fresh littlefs instance on the Linux simulator,
 * performs a filesystem operation, then unmounts and verifies the result.
 *
 * Flash backing file: /tmp/norfx_test_lfs.bin
 */

#include "unity.h"
#include "nor_fx.h"
#include "nor_fx_lfs.h"
#include "port_linux.h"
#include "lfs.h"

#include <string.h>

/* --------------------------------------------------------------------------
 * Test fixture
 * -------------------------------------------------------------------------- */

static norfx_sim_ctx_t sim;
static struct norfx_device dev;
static norfx_lfs_ctx_t lfs_ctx;
static lfs_t lfs;

/** Populated once in setUp(), reused across all tests. */
static struct lfs_config cfg;

void setUp(void)
{
    norfx_sim_init(&sim, "/tmp/norfx_test_lfs.bin", NORFX_SIM_FLASH_SIZE);
    norfx_sim_chip_erase(&sim);

    dev.context           = &sim;
    dev.spi_chip_select   = norfx_sim_cs_select;
    dev.spi_chip_deselect = norfx_sim_cs_deselect;
    dev.spi_write         = norfx_sim_spi_write;
    dev.spi_read          = norfx_sim_spi_read;
    dev.get_tick_ms       = norfx_sim_get_tick_ms;
    dev.delay_ms          = norfx_sim_delay_ms;

    lfs_ctx.dev = &dev;

    cfg.context        = &lfs_ctx;
    cfg.read           = norfx_lfs_read;
    cfg.prog           = norfx_lfs_prog;
    cfg.erase          = norfx_lfs_erase;
    cfg.sync           = norfx_lfs_sync;
    cfg.read_size      = 1;
    cfg.prog_size      = NORFX_PAGE_SIZE;
    cfg.block_size     = NORFX_SECTOR_SIZE;
    cfg.block_count    = NORFX_BLOCK_COUNT;
    cfg.cache_size     = NORFX_PAGE_SIZE;
    cfg.lookahead_size = 16;
    cfg.block_cycles   = 500;
}

void tearDown(void)
{
    norfx_sim_deinit(&sim);
}

/* --------------------------------------------------------------------------
 * Helper — format + mount a fresh filesystem
 * -------------------------------------------------------------------------- */
static void mount_fresh(void)
{
    int err = lfs_format(&lfs, &cfg);
    TEST_ASSERT_EQUAL_MESSAGE(0, err, "lfs_format failed");

    err = lfs_mount(&lfs, &cfg);
    TEST_ASSERT_EQUAL_MESSAGE(0, err, "lfs_mount failed");
}

/* --------------------------------------------------------------------------
 * Tests
 * -------------------------------------------------------------------------- */

/**
 * @brief Format and mount should succeed on an erased flash.
 */
void test_lfs_format_and_mount(void)
{
    int err = lfs_format(&lfs, &cfg);
    TEST_ASSERT_EQUAL(0, err);

    err = lfs_mount(&lfs, &cfg);
    TEST_ASSERT_EQUAL(0, err);

    lfs_unmount(&lfs);
}

/**
 * @brief Mount after unmount should succeed (superblock persists).
 */
void test_lfs_remount(void)
{
    mount_fresh();
    lfs_unmount(&lfs);

    int err = lfs_mount(&lfs, &cfg);
    TEST_ASSERT_EQUAL_MESSAGE(0, err, "remount failed");

    lfs_unmount(&lfs);
}

/**
 * @brief Write a file and read it back — content must match.
 */
void test_lfs_write_and_read_file(void)
{
    mount_fresh();

    const char *filename  = "hello.txt";
    const char *write_buf = "Hello, nor_fx!";
    char        read_buf[32];

    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, filename,
                            LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    TEST_ASSERT_EQUAL(0, err);

    lfs_ssize_t written = lfs_file_write(&lfs, &file, write_buf,
                                         (lfs_size_t)strlen(write_buf));
    TEST_ASSERT_EQUAL((lfs_ssize_t)strlen(write_buf), written);

    lfs_file_close(&lfs, &file);

    err = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
    TEST_ASSERT_EQUAL(0, err);

    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, read_buf,
                                           sizeof(read_buf) - 1);
    TEST_ASSERT_EQUAL((lfs_ssize_t)strlen(write_buf), bytes_read);

    read_buf[bytes_read] = '\0';
    TEST_ASSERT_EQUAL_STRING(write_buf, read_buf);

    lfs_file_close(&lfs, &file);
    lfs_unmount(&lfs);
}

/**
 * @brief Written file must survive an unmount/remount cycle.
 */
void test_lfs_file_persists_after_remount(void)
{
    mount_fresh();

    const char *filename  = "persist.txt";
    const char *write_buf = "persistent data";

    lfs_file_t file;
    lfs_file_open(&lfs, &file, filename,
                  LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    lfs_file_write(&lfs, &file, write_buf, (lfs_size_t)strlen(write_buf));
    lfs_file_close(&lfs, &file);
    lfs_unmount(&lfs);

    /* Remount and verify */
    int err = lfs_mount(&lfs, &cfg);
    TEST_ASSERT_EQUAL(0, err);

    char read_buf[32];
    err = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
    TEST_ASSERT_EQUAL_MESSAGE(0, err, "file not found after remount");

    lfs_ssize_t n = lfs_file_read(&lfs, &file, read_buf, sizeof(read_buf) - 1);
    read_buf[n] = '\0';
    TEST_ASSERT_EQUAL_STRING(write_buf, read_buf);

    lfs_file_close(&lfs, &file);
    lfs_unmount(&lfs);
}

/**
 * @brief lfs_stat on an existing file should return correct size.
 */
void test_lfs_stat_returns_correct_size(void)
{
    mount_fresh();

    const char *filename  = "stat_test.txt";
    const char *write_buf = "1234567890";  /* 10 bytes */

    lfs_file_t file;
    lfs_file_open(&lfs, &file, filename,
                  LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    lfs_file_write(&lfs, &file, write_buf, (lfs_size_t)strlen(write_buf));
    lfs_file_close(&lfs, &file);

    struct lfs_info info;
    int err = lfs_stat(&lfs, filename, &info);
    TEST_ASSERT_EQUAL(0, err);
    TEST_ASSERT_EQUAL(LFS_TYPE_REG, info.type);
    TEST_ASSERT_EQUAL((lfs_size_t)strlen(write_buf), info.size);

    lfs_unmount(&lfs);
}

/* --------------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------------- */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_lfs_format_and_mount);
    RUN_TEST(test_lfs_remount);
    RUN_TEST(test_lfs_write_and_read_file);
    RUN_TEST(test_lfs_file_persists_after_remount);
    RUN_TEST(test_lfs_stat_returns_correct_size);

    return UNITY_END();
}
