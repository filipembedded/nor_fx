# NORFX status snapshot (2026-04-05)

## Scope
`nor-flash-xplat/driver/nor_fx.c` i `nor-flash-xplat/driver/nor_fx.h`

## Trenutno urađeno
- `norfx_reset()`
  - Validira `dev` i callback-e.
  - Šalje `INST_ENABLE_RESET` + `INST_RESET_DEVICE`.
  - Urađen `chip_deselect` i propagacija statusa iz `check_flash_ready()`.
- `norfx_read_status_reg()`
  - Potpis usklađen (`uint8_t *status_reg`) i u `.h` i u `.c`.
  - Validacija ulaza + kompletan `CS` tok (`select -> write -> read -> deselect`).
- `check_flash_ready()`
  - Polling `SR1` (`NORFX_WIP_MASK`) sa timeout-om (`NORFX_READY_TIMEOUT_MS`).
  - Koristi `get_tick_ms` i `delay_ms` callback-e iz `struct norfx_device`.
- `norfx_write_enable()`
  - Slanje `INST_WRITE_ENABLE`, cleanup i `check_flash_ready()`.
- `norfx_write_disable()`
  - Slanje `INST_WRITE_DISABLE`, cleanup i `check_flash_ready()`.

## Trenutni otvoreni problemi
1. `norfx_write_enable()` i `norfx_write_disable()` ne verifikuju `WEL` bit (`SR1 bit1`).
   - Trenutno se proverava samo `WIP` (busy/ready), što nije isto što i write-latch stanje.
2. `norfx_erase_sector()` je prazna funkcija.
3. `norfx_read_id()`, `norfx_read()`, `norfx_fast_read()` imaju `//TODO: Impl`.
4. `NORFX_READY_TIMEOUT_MS` je trenutno globalno `50 ms`.
   - To je često OK za kratke operacije, ali nije dovoljno za erase/program tokove.

## Predlog prioriteta za sutra (redosled)
1. Dodati `WEL` proveru posle `write_enable` i `write_disable`.
2. Implementirati `norfx_erase_sector()`:
   - `write_enable -> erase_cmd+addr -> check_flash_ready -> write_disable (opciono)`.
3. Implementirati `norfx_read_id()` (bar `ID_JEDEC`) za sanity check čipa.
4. Implementirati `norfx_read()` i `norfx_fast_read()`.
5. Razdvojiti timeout konstante po operaciji (reset/read/program/erase).

## Predlog helper makroa/bitova
- `NORFX_WIP_MASK` = `0x01u` (već postoji)
- `NORFX_WEL_MASK` = `0x02u` (dodati)

## Brzi acceptance kriterijumi
- `write_enable` vraća uspeh samo ako je `WEL=1`.
- `write_disable` vraća uspeh samo ako je `WEL=0`.
- `erase_sector` ima timeout zaštitu i ne ostavlja aktivan `CS` ni na error putu.
- `read_status_reg` ostaje usklađen između `.h` i `.c`.

## Napomena
Nije rađeno uvodjenje QSPI/OSPI u ovoj fazi — fokus ostaje `SPI-only v1`.
