# NORFX status snapshot (2026-04-10)

## Scope
`nor-flash-xplat/driver/nor_fx.c` i `nor-flash-xplat/driver/nor_fx.h`

## Trenutno urađeno (2026-04-10)
- `norfx_reset()` — 9/10, solidno
- `norfx_read_status_reg()` — 9/10, solidno
- `norfx_write_enable()` — 8/10, nema WEL verifikacije (videti otvorene probleme)
- `norfx_write_disable()` — 8/10, nema WEL verifikacije (videti otvorene probleme)
- `norfx_read_id()` — 9/10, podržava sve 4 ID varijante (`JEDEC`, `MFR_DEV`, `RELEASE_PD`, `UNIQUE`)
  - Interni helperi: `get_id_size()`, `convert_buf_to_id()`, `check_id_kind()`
- `norfx_read()` — 9/10, solidno
- `check_flash_ready()` — 9/10, WIP polling sa timeout-om i callback-ima

## Otvoreni problemi
1. **`norfx_write_enable/disable` — nema WEL verifikacije** (prioritet kada se bude radilo)
   - Detaljan predlog implementacije:
     ```c
     #define NORFX_WEL_MASK  0x02u

     // write_enable — posle check_flash_ready:
     uint8_t sr = 0;
     enum norfx_status st = norfx_read_status_reg(dev, &sr);
     if (st != NORFX_SUCCESS) return st;
     if ((sr & NORFX_WEL_MASK) == 0u) return NORFX_ERROR;

     // write_disable — posle check_flash_ready:
     uint8_t sr = 0;
     enum norfx_status st = norfx_read_status_reg(dev, &sr);
     if (st != NORFX_SUCCESS) return st;
     if ((sr & NORFX_WEL_MASK) != 0u) return NORFX_ERROR;
     ```
   - Opciono: centralizovati u `static enum norfx_status verify_wel(dev, uint8_t expected_wel)`
     koji prima `1u` za enable i `0u` za disable.
   - Koristiti i u `erase_sector`/`page_program` kao preduslov.

2. **`norfx_fast_read()` — nije implementirana** (`//TODO: Impl`)

3. **`norfx_erase_sector()` — prazna funkcija**
   - Tok: `write_enable -> erase_cmd+addr -> check_flash_ready`
   - Timeout za erase je duži od `50ms` — razdvojiti konstantu.

4. **`NORFX_READY_TIMEOUT_MS = 50ms` je globalno** — premalo za erase/program.
   - Predlog: odvojene konstante `NORFX_ERASE_TIMEOUT_MS`, `NORFX_PROGRAM_TIMEOUT_MS`.

5. **Sitnice u `norfx_read`**:
   - `tx_buf[4]` nije inicijalizovan — dodati `= {0}`.
   - `size == 0` nije zaštićen.

6. **`stdbool.h`** je uključen ali `bool` se ne koristi — ukloniti.

## Predlog prioriteta (sledeći korak)
1. Implementirati `norfx_erase_sector()`.
2. Implementirati `norfx_fast_read()`.
3. Dodati WEL verifikaciju u `write_enable/disable`.
4. Razdvojiti timeout konstante.
5. Sitne ispravke u `norfx_read`.

## Makroi/bitovi
- `NORFX_WIP_MASK` = `0x01u` ✅ (postoji)
- `NORFX_WEL_MASK` = `0x02u` ⬜ (dodati u `nor_fx.h`)

## Napomena
Nije rađeno uvođenje QSPI/OSPI u ovoj fazi — fokus ostaje `SPI-only v1`.
