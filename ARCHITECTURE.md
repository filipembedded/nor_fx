# nor_fx — Architecture & Build System

## 1. Goals

- **Cross-platform**: core driver has zero platform dependencies — all I/O is
  injected via function pointers in `struct norfx_device`.
- **Self-contained tests**: the library can build and test itself on a Linux
  host with no hardware required, using a file-backed NOR flash simulator.
- **Clean submodule integration**: when consumed as a git submodule by a
  parent project (e.g. `stm32-nvs-demos`), only explicitly requested optional
  components are built — the parent sees only the targets it asks for.
- **No HAL leakage**: platform headers (e.g. `stm32f4xx_hal.h`) never appear
  in public `.h` files; they are confined to the corresponding `.c` port file.

---

## 2. Repository Layout

```
nor_fx/
├── CMakeLists.txt                  # Root — defines project, exposes targets
│
├── driver/
│   ├── nor_fx.h                    # Public API — no platform deps
│   ├── nor_fx.c                    # Implementation
│   └── CMakeLists.txt              # target: norfx_driver (STATIC)
│
├── fs/
│   └── littlefs/
│       ├── nor_flash_lfs.h         # littlefs <-> norfx_driver glue
│       ├── nor_flash_lfs.c
│       └── CMakeLists.txt          # target: norfx_lfs (STATIC)
│                                   # requires: lfs (provided by parent or submodule)
│
├── port/
│   ├── stm32/
│   │   └── f4/
│   │       ├── stm32f4x_port.h     # NO HAL includes — uses void* for HAL types
│   │       ├── stm32f4x_port.c     # includes stm32f4xx_hal.h HERE only
│   │       └── CMakeLists.txt      # target: norfx_port_stm32 (+ alias norfx_port_stm32_f4)
│   └── native_sim/linux/
│       ├── port_linux.h            # Linux simulator public API
│       ├── port_linux.c            # mmap-backed flash simulator
│       └── CMakeLists.txt          # target: norfx_port_linux (+ alias norfx_port_native_linux)
│
└── tests/
  ├── CMakeLists.txt              # only included when NORFX_BUILD_TESTS=ON
    ├── unity/                      # Unity test framework (git submodule)
    │   ├── unity.c
    │   └── unity.h
    ├── test_norfx_reset.c
    ├── test_norfx_read.c
    ├── test_norfx_fast_read.c
    ├── test_norfx_page_program.c
    ├── test_norfx_erase_sector.c
    └── test_norfx_write.c
```

---

## 3. CMake Target Graph

```
norfx_driver          (STATIC — driver/nor_fx.c)
    │
    ├── norfx_port_stm32    (STATIC — port/stm32/f4/stm32f4x_port.c)
    │       [explicitly enabled STM32F4 port]
    │       [public alias: norfx_port_stm32_f4]
    │
    ├── norfx_port_linux    (STATIC — port/native_sim/linux/port_linux.c)
    │       [explicitly enabled host port]
    │       [public alias: norfx_port_native_linux]
    │
    ├── norfx_lfs           (STATIC — fs/littlefs/nor_flash_lfs.c)
    │       [optional, requires NORFX_BUILD_LFS=ON]
    │
    └── tests/              (executables — host only, built when NORFX_BUILD_TESTS=ON)
            test_norfx_reset
            test_norfx_read
            test_norfx_fast_read
            test_norfx_page_program
            test_norfx_erase_sector
            test_norfx_write
            test_norfx_lfs        [only when norfx_lfs is enabled]
```

---

## 4. Build Modes

### 4a. Standalone — develop & test the library on Linux

```bash
cd nor_fx
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

What gets built:
- `norfx_driver`
- `norfx_port_linux` / `norfx_port_native_linux`
- `norfx_lfs`
- all test executables
- Unity

What does NOT get built:
- `norfx_port_stm32` / `norfx_port_stm32_f4`

### 4b. Submodule — consumed by parent firmware project

```cmake
# stm32-nvs-demos/CMakeLists.txt  (or STM32CubeIDE / Makefile equivalent)
set(NORFX_BUILD_PORT_STM32_F4 ON CACHE BOOL "" FORCE)
add_subdirectory(libraries/nor_fx)

target_link_libraries(norfx_port_stm32 PRIVATE stm32_hal)

target_link_libraries(my_firmware PRIVATE
    norfx_driver
    norfx_port_stm32_f4
)
```

What gets built:
- `norfx_driver`
- `norfx_port_stm32` / `norfx_port_stm32_f4` (because parent explicitly enabled `NORFX_BUILD_PORT_STM32_F4`)

What does NOT get built:
- `tests/` unless explicitly enabled
- `norfx_port_linux` / `norfx_port_native_linux` unless explicitly enabled
- `norfx_lfs` unless explicitly enabled

---

## 5. Port Rules

### STM32 port — HAL isolation pattern

```
port_stm32.h  ← public, NO #include "stm32xxx_hal.h"
                uses void* for SPI_HandleTypeDef* and GPIO_TypeDef*

port_stm32.c  ← private, #include "stm32xxx_hal.h" HERE ONLY
                casts void* back to HAL types inside each function
```

This means:
- `port_stm32.h` can be included anywhere without triggering a HAL dependency.
- The HAL header path is only needed when compiling `port_stm32.c`, which
  only happens when the STM32 port target is explicitly enabled.

### Linux simulator port

- Flash image backed by a file (default: `flash_sim.bin`).
- `mmap(MAP_SHARED)` — zero-copy read/write, persists across runs.
- Correct NOR behaviour: page program ANDs bytes (bits only go 0→0, not 0→1),
  sector erase resets to 0xFF.
- WIP and WEL bits in the simulated Status Register 1.
- JEDEC ID returns W25Q128JV values: `0xEF 0x70 0x18`.

---

## 6. Test Strategy

Each test file:
1. Calls `norfx_sim_init()` to create a clean flash image.
2. Constructs a `struct norfx_device` wired to the Linux simulator callbacks.
3. Exercises one driver function via Unity assertions.
4. Calls `norfx_sim_deinit()` in teardown.

Test categories:

| File                        | What is tested                                      |
|-----------------------------|-----------------------------------------------------|
| `test_norfx_reset.c`        | Reset sequence, WIP polling, timeout                |
| `test_norfx_read.c`         | Normal read, address calculation, erased content    |
| `test_norfx_fast_read.c`    | Fast read, dummy byte handling                      |
| `test_norfx_page_program.c` | Write to erased page, AND behaviour, boundary       |
| `test_norfx_erase_sector.c` | 4 KB sector erase, address alignment, WEL lifecycle |
| `test_norfx_write.c`        | Read-modify-write across sector boundaries          |

---

## 7. Adding a New Port

1. Create `port/<platform>/port_<platform>.c/.h`.
2. Follow the HAL isolation rule: no platform headers in `.h`.
3. Implement all six `struct norfx_device` callbacks:
   - `spi_chip_select`
   - `spi_chip_deselect`
   - `spi_write`
   - `spi_read`
   - `get_tick_ms`
   - `delay_ms`
4. Add `port/<platform>/CMakeLists.txt` as an explicit optional target.
5. Document the port in this file under a new section.
