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
int boot_disc_file_index(const BootDisc *disc, const char *path);
int boot_disc_file_at(const BootDisc *disc, int index, uint32_t *offset, uint32_t *size);
void boot_disc_dol(const BootDisc *disc, uint32_t *offset, uint32_t *size);
int boot_switchwaker_assets(SceUID log, const char *path);
int boot_switchwaker_archives(SceUID log, const char *path);
enum { BOOT_BANNER_WIDTH = 96, BOOT_BANNER_HEIGHT = 32 };
int boot_read_banner(SceUID log, const char *path, uint32_t *pixels);
int boot_probe_system(SceUID log);
int boot_probe_aram(SceUID log);
int boot_probe_scheduler(SceUID log);
int boot_probe_game_dvd(SceUID log);
int boot_gamepad_init(SceUID log);
int boot_gamepad_poll(SceUID log);
int boot_mount_game_dvd(SceUID log, const char *path);
void boot_unmount_game_dvd(void);
int boot_probe_stop_scheduler(SceUID log);
void boot_probe_exit(SceUID log, int status) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
