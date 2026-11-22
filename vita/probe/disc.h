#pragma once

#include <psp2/types.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct BootDisc BootDisc;
BootDisc *boot_disc_open(SceUID log, const char *path);
void boot_disc_close(BootDisc *disc);
int boot_disc_read(const BootDisc *disc, uint64_t offset, void *output, size_t bytes);
int boot_disc_file(const BootDisc *disc, const char *path, uint32_t *offset, uint32_t *size);
void boot_disc_dol(const BootDisc *disc, uint32_t *offset, uint32_t *size);
int boot_switchwaker_assets(SceUID log, const char *path);
void boot_probe_exit(SceUID log, int status) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
