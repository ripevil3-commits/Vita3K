// Vita3K PS4 homebrew entry point (experimental).
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "interface.h"

#include <app/functions.h>
#include <app/session_controller.h>
#include <config/functions.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <modules/module_parent.h>
#include <packages/license.h>
#include <sdl-frontend/session.h>
#include <util/exit_code.h>
#include <util/fs.h>
#include <util/log.h>

#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[Vita3K-PS4] experimental entry point\n");

    Root root_paths;
    const bool portable = app::init_paths(root_paths);
    // All Vita3K writable data stays under /data/Vita3K on PS4. Create each root before
    // logging/config code opens files there; /app0 remains the read-only package root.
    fs::create_directories(root_paths.get_vita_fs_path());
    fs::create_directories(root_paths.get_log_path());
    fs::create_directories(root_paths.get_config_path());
    fs::create_directories(root_paths.get_shared_path());
    fs::create_directories(root_paths.get_cache_path());

    if (logging::init(root_paths, true) != Success) {
        std::printf("[Vita3K-PS4] failed to initialize logging\n");
        return static_cast<int>(InitConfigFailed);
    }

    Config cfg{};
    EmuEnvState emuenv;
    const auto config_result = config::init_config(cfg, argc, argv, root_paths, portable);
    if (config_result != Success) {
        LOG_ERROR("PS4 entry point could not initialize config (result {}).", static_cast<int>(config_result));
        return static_cast<int>(config_result);
    }

    // The PS4 target has no Qt desktop frontend. Initially reuse Vita3K's SDL session path
    // and require an installed title ID; library browsing and the PS4 Vulkan WSI are separate
    // integration tasks. Never pretend this shell is a working emulator before they are tested.
    cfg.frontend = Frontend::sdl;
    cfg.backend_renderer = "Vulkan";
    cfg.audio_backend = "SDL";
    cfg.current_config.backend_renderer = "Vulkan";
    cfg.current_config.audio_backend = "SDL";

    // PS4 application launches generally cannot pass arbitrary desktop CLI arguments. Until a
    // native library browser/package installer exists, a user can put one installed title ID
    // (for example PCSA00000) in /data/Vita3K/config/launch_app.txt.
    if (!cfg.run_app_path || cfg.run_app_path->empty()) {
        const auto launch_file_path = root_paths.get_config_path() / "launch_app.txt";
        std::ifstream launch_file(launch_file_path.c_str());
        std::string installed_title_id;
        if (launch_file && std::getline(launch_file, installed_title_id) && !installed_title_id.empty())
            cfg.run_app_path = installed_title_id;
    }

    if (!cfg.run_app_path || cfg.run_app_path->empty()) {
        LOG_ERROR("No title selected. Add an installed title ID to /data/Vita3K/config/launch_app.txt.");
        LOG_ERROR("The native PS4 library browser and package installer are not connected yet.");
        return 2;
    }

    fs::create_directories(cfg.get_vita_fs_path());
    if (!app::init(emuenv, cfg, root_paths)) {
        LOG_ERROR("Emulated Vita environment initialization failed.");
        return 1;
    }

    init_libraries(emuenv);
    if (!app::init_apps_list(emuenv)) {
        LOG_ERROR("Failed to initialize the Vita app list.");
        return 1;
    }
    app::load_users(emuenv);

    app::AppSessionController session(emuenv);
    const int result = sdl_frontend::run(
        emuenv,
        session,
        AppLaunchRequest{ .app_path = *emuenv.cfg.run_app_path });

    LOG_INFO("Vita3K PS4 frontend stopped with result {}.", result);
    return result;
}
