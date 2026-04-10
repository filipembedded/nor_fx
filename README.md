# nor_fx

## Intro

A cross-platform [NOR flash](https://en.wikipedia.org/wiki/Flash_memory#NOR_flash) driver library designed for embedded devices. The driver is hardware-agnostic — all platform I/O is supplied by the application via function pointers, making it portable across any MCU or operating system. The library also ships a Linux-hosted flash simulator, enabling full driver testing on a development machine without any hardware.

## Design

This library is designed in a layered manner:

```text
+-------------------+
|    application    | - Embedded application using NOR flash as a storage medium
+-------------------+
          ^
          |
+-------------------+
|    file system    | - Optional: littlefs glue layer (fs/littlefs/)
+-------------------+
          ^
          |
+-------------------+
|      driver       | - NOR flash driver: constant, cross-platform (driver/)
+-------------------+
          ^
          |
+-------------------+
|       port        | - Platform port: user configurable, depends on hardware (port/)
+-------------------+
```

The **driver** layer provides a unified API that remains identical across all supported platforms. All hardware interaction is abstracted behind six function pointers held in a `struct norfx_device`:

| Callback | Description |
|---|---|
| `spi_chip_select` | Assert chip-select (CS low) |
| `spi_chip_deselect` | De-assert chip-select (CS high) |
| `spi_write` | Transmit bytes over SPI |
| `spi_read` | Receive bytes over SPI |
| `get_tick_ms` | Return a millisecond tick counter |
| `delay_ms` | Block for N milliseconds |

The **port** layer connects a specific platform to the driver by implementing those six callbacks. The optional **file system** layer provides a ready-made [littlefs](https://github.com/littlefs-project/littlefs) integration on top of the driver.

## Driver API

```c
enum norfx_status norfx_reset(struct norfx_device *dev);

enum norfx_status norfx_read_id(struct norfx_device *dev,
                                enum norfx_id_kind id,
                                uint32_t *id_val);

enum norfx_status norfx_read(struct norfx_device *dev,
                             uint32_t start_page, uint8_t offset,
                             uint32_t size, uint8_t *rx_buf);

enum norfx_status norfx_fast_read(struct norfx_device *dev,
                                  uint32_t start_page, uint8_t offset,
                                  uint32_t size, uint8_t *rx_buf);

enum norfx_status norfx_erase_sector(struct norfx_device *dev,
                                     uint16_t num_sector);

enum norfx_status norfx_page_program(struct norfx_device *dev,
                                     uint32_t page, uint16_t offset,
                                     uint32_t size, uint8_t *data);

enum norfx_status norfx_write(struct norfx_device *dev,
                              uint32_t page, uint16_t offset,
                              uint32_t size, uint8_t *data,
                              uint8_t *scratch_buf);
```

> **Note:** `norfx_write` performs an automatic read-modify-write cycle and is intended for raw flash usage. Do **not** use it with a file system such as littlefs — pass `norfx_page_program` and `norfx_erase_sector` directly to the littlefs block device callbacks instead.

## Porting the Library

Create a `.c` / `.h` pair under `port/<platform>/` and implement the six callbacks. Rules:

- **No platform headers in `.h`** — use `void *` for HAL handle types. Cast inside the `.c` file only.
- Implement a `struct norfx_<platform>_ctx` to carry the hardware handles passed through `void *context`.

A complete port for STM32 (HAL SPI) is located at `port/stm32/`.  
A Linux simulation port (file-backed, no hardware needed) is located at `port/native_sim/linux/`.

Minimal port skeleton:

```c
/* port_myplatform.h */
#include "nor_fx.h"

struct norfx_myplatform_ctx {
    void    *hspi;      /* SPI handle — void* to avoid HAL header in .h */
    void    *cs_port;   /* GPIO port  — void* for the same reason        */
    uint16_t cs_pin;
};

enum norfx_status norfx_myplatform_cs_select(void *context);
enum norfx_status norfx_myplatform_cs_deselect(void *context);
enum norfx_status norfx_myplatform_spi_write(void *context, uint8_t *data, uint16_t size);
enum norfx_status norfx_myplatform_spi_read(void *context, uint8_t *data, uint16_t size);
uint32_t          norfx_myplatform_get_tick_ms(void *context);
void              norfx_myplatform_delay_ms(void *context, uint32_t delay);
```

```c
/* port_myplatform.c */
#include "platform_hal.h"   /* HAL header lives here only */
#include "port_myplatform.h"

enum norfx_status norfx_myplatform_spi_write(void *context,
                                             uint8_t *data,
                                             uint16_t size)
{
    struct norfx_myplatform_ctx *ctx = context;
    /* cast void* back to real HAL type here */
    ...
}
```

## Linux Simulator

The library includes a file-backed NOR flash simulator for host testing (`port/native_sim/linux/`). It models correct NOR flash behaviour:

- **Read** — returns flash content directly.
- **Page program** — ANDs incoming bytes with existing content (bits can only go `1→0`, not `0→1` without an erase).
- **Sector erase** — resets 4 KB to `0xFF`.
- **WIP / WEL bits** — simulated in Status Register 1.
- **JEDEC ID** — returns Winbond W25Q128JV values (`0xEF 0x70 0x18`).
- **Persistent state** — backed by a file on disk; state survives across runs.

Basic usage:

```c
#include "nor_fx.h"
#include "port_linux.h"

norfx_sim_ctx_t ctx;
norfx_sim_init(&ctx, "flash.bin", NORFX_SIM_FLASH_SIZE);

struct norfx_device dev = {
    .context          = &ctx,
    .spi_chip_select   = norfx_sim_cs_select,
    .spi_chip_deselect = norfx_sim_cs_deselect,
    .spi_write         = norfx_sim_spi_write,
    .spi_read          = norfx_sim_spi_read,
    .get_tick_ms       = norfx_sim_get_tick_ms,
    .delay_ms          = norfx_sim_delay_ms,
};

norfx_reset(&dev);
/* ... use driver normally ... */
norfx_sim_deinit(&ctx);
```

## Building and Running Tests

The library uses CMake. Tests are only built when the library is the top-level CMake project (i.e. not consumed as a submodule).

```bash
git submodule update --init --recursive   # pulls Unity test framework
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected output:

```
100% tests passed, 0 tests failed out of 6
```

## Submodule Integration

When consumed as a git submodule, tests and host-only targets are automatically excluded:

```cmake
# Parent project CMakeLists.txt
add_subdirectory(libraries/nor_fx)

target_link_libraries(my_firmware PRIVATE
    norfx_driver
    norfx_port_stm32
)
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for a full description of the CMake target graph and design decisions.

## Demos

A complete demo of the **nor_fx** library running on STM32 can be found in the following [repository](https://github.com/filipembedded/stm32-nvs-demos).

## License

This project is licensed under the BSD 3-Clause License — see the [LICENSE](LICENSE) file for details.
