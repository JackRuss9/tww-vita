#pragma once

#include <nod.h>
#include <psp2/types.h>

#include <cstdlib>
#include <format>
#include <utility>

bool tww_vita_open_dvd_stream(const char *path, NodDiscStream *stream);
void tww_vita_set_dvd_log(SceUID log);
void tww_vita_dvd_log(const char *severity, const char *message);

struct VitaDvdLog {
    template<class... Args>
    void error(std::format_string<Args...> message, Args&&... args) const
    {
        tww_vita_dvd_log("error", std::format(message, std::forward<Args>(args)...).c_str());
    }
    template<class... Args>
    void warn(std::format_string<Args...> message, Args&&... args) const
    {
        tww_vita_dvd_log("warning", std::format(message, std::forward<Args>(args)...).c_str());
    }
    template<class... Args>
    [[noreturn]] void fatal(std::format_string<Args...> message, Args&&... args) const
    {
        tww_vita_dvd_log("fatal", std::format(message, std::forward<Args>(args)...).c_str());
        std::abort();
    }
};
