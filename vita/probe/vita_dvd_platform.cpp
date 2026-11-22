#include "vita_dvd_platform.h"
#include "disc.h"
#include "gpu.h"

#include <algorithm>
#include <cstdio>
#include <psp2/io/fcntl.h>

namespace {
constexpr uint64_t GameCubeDiscSize = 1459978240;
SceUID log_fd = -1;

int64_t stream_length(void *context)
{
    return static_cast<int64_t>(std::min(GameCubeDiscSize,
        boot_disc_addressable_bytes(static_cast<BootDisc *>(context))));
}

int64_t stream_read(void *context, uint64_t offset, void *out, size_t bytes)
{
    const uint64_t limit = static_cast<uint64_t>(stream_length(context));
    if (offset >= limit)
        return 0;
    const auto count = static_cast<size_t>(std::min<uint64_t>(bytes, limit - offset));
    return boot_disc_read(static_cast<BootDisc *>(context), offset, out, count) == 0
        ? static_cast<int64_t>(count) : -1;
}

void stream_close(void *context)
{
    boot_disc_close(static_cast<BootDisc *>(context));
}
}

bool tww_vita_open_dvd_stream(const char *path, NodDiscStream *stream)
{
    BootDisc *disc = boot_disc_open(log_fd, path);
    if (!disc)
        return false;
    *stream = {
        .user_data = disc,
        .read_at = stream_read,
        .stream_len = stream_length,
        .close = stream_close,
    };
    return true;
}

void tww_vita_set_dvd_log(SceUID log)
{
    log_fd = log;
}

void tww_vita_dvd_log(const char *severity, const char *message)
{
    if (log_fd >= 0) {
        probe_log(log_fd, "dvd: %s: %s\n", severity, message);
        sceIoSyncByFd(log_fd, 0);
    } else {
        std::fprintf(stderr, "dvd: %s: %s\n", severity, message);
    }
}
