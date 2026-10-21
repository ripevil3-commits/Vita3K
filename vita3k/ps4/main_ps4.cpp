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

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[Vita3K-PS4] experimental entry point\n");

    Root root_paths;
    const bool portable = app::init_paths(root_paths);
    fs::create_directories(root_paths.get_vita_fs_path());

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
    cfg.current_config.backend_renderer = "Vulkan";

    if (!cfg.run_app_path || cfg.run_app_path->empty()) {
        LOG_ERROR("PS4 prototype currently needs an installed title path (the -r/--installed-path argument).");
        LOG_ERROR("A native PS4 library browser and package installer are not connected yet.");
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
