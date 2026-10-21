// Vita3K PS4 display host. Requires a RADV/VideoOut implementation of VK_KHR_display.
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "frame_host.h"

#include <cstdio>
#include <string>
#include <vector>

namespace sdl_frontend {
namespace {

template <typename T>
T load_instance_function(VkInstance instance, const char *name) {
    return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
}

void log_vulkan_failure(const char *operation, VkResult result) {
    std::fprintf(stderr, "[Vita3K-PS4] %s failed with VkResult %d\n",
                 operation, static_cast<int>(result));
}

} // namespace

FrameHost::FrameHost() = default;

renderer::DisplayHandle FrameHost::handle() const {
    return std::monostate{};
}

const renderer::VulkanSurfaceProvider *FrameHost::vulkan_surface_provider() const {
    return this;
}

std::vector<const char *> FrameHost::instance_extensions() const {
    // The external PS4 RADV WSI must implement these extensions. This is not a desktop Vulkan
    // surface and must never fall back to X11, Wayland, or an SDL video driver.
    return {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME};
}

VkSurfaceKHR FrameHost::create_surface(VkInstance instance) const {
    const auto enumerate_devices =
        load_instance_function<PFN_vkEnumeratePhysicalDevices>(instance, "vkEnumeratePhysicalDevices");
    const auto enumerate_displays =
        load_instance_function<PFN_vkGetPhysicalDeviceDisplayPropertiesKHR>(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR");
    const auto enumerate_planes =
        load_instance_function<PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR>(instance, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR");
    const auto supported_displays =
        load_instance_function<PFN_vkGetDisplayPlaneSupportedDisplaysKHR>(instance, "vkGetDisplayPlaneSupportedDisplaysKHR");
    const auto enumerate_modes =
        load_instance_function<PFN_vkGetDisplayModePropertiesKHR>(instance, "vkGetDisplayModePropertiesKHR");
    const auto plane_capabilities =
        load_instance_function<PFN_vkGetDisplayPlaneCapabilitiesKHR>(instance, "vkGetDisplayPlaneCapabilitiesKHR");
    const auto create_surface =
        load_instance_function<PFN_vkCreateDisplayPlaneSurfaceKHR>(instance, "vkCreateDisplayPlaneSurfaceKHR");

    if (!enumerate_devices || !enumerate_displays || !enumerate_planes ||
        !supported_displays || !enumerate_modes || !plane_capabilities || !create_surface) {
        std::fprintf(stderr,
            "[Vita3K-PS4] RADV WSI is missing one or more VK_KHR_display entry points. "
            "Link the PS4 VideoOut WSI with the Vita3K target.\n");
        return VK_NULL_HANDLE;
    }

    uint32_t device_count = 0;
    VkResult result = enumerate_devices(instance, &device_count, nullptr);
    if (result != VK_SUCCESS || device_count == 0) {
        log_vulkan_failure("vkEnumeratePhysicalDevices(count)", result);
        return VK_NULL_HANDLE;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    result = enumerate_devices(instance, &device_count, devices.data());
    if (result != VK_SUCCESS || device_count == 0) {
        log_vulkan_failure("vkEnumeratePhysicalDevices(list)", result);
        return VK_NULL_HANDLE;
    }

    const VkPhysicalDevice physical_device = devices.front();
    uint32_t display_count = 0;
    result = enumerate_displays(physical_device, &display_count, nullptr);
    if (result != VK_SUCCESS || display_count == 0) {
        log_vulkan_failure("vkGetPhysicalDeviceDisplayPropertiesKHR(count)", result);
        return VK_NULL_HANDLE;
    }

    std::vector<VkDisplayPropertiesKHR> displays(display_count);
    result = enumerate_displays(physical_device, &display_count, displays.data());
    if (result != VK_SUCCESS || display_count == 0) {
        log_vulkan_failure("vkGetPhysicalDeviceDisplayPropertiesKHR(list)", result);
        return VK_NULL_HANDLE;
    }

    uint32_t plane_count = 0;
    result = enumerate_planes(physical_device, &plane_count, nullptr);
    if (result != VK_SUCCESS || plane_count == 0) {
        log_vulkan_failure("vkGetPhysicalDeviceDisplayPlanePropertiesKHR(count)", result);
        return VK_NULL_HANDLE;
    }

    std::vector<VkDisplayPlanePropertiesKHR> planes(plane_count);
    result = enumerate_planes(physical_device, &plane_count, planes.data());
    if (result != VK_SUCCESS || plane_count == 0) {
        log_vulkan_failure("vkGetPhysicalDeviceDisplayPlanePropertiesKHR(list)", result);
        return VK_NULL_HANDLE;
    }

    for (const VkDisplayPropertiesKHR &display : displays) {
        uint32_t mode_count = 0;
        result = enumerate_modes(physical_device, display.display, &mode_count, nullptr);
        if (result != VK_SUCCESS || mode_count == 0)
            continue;

        std::vector<VkDisplayModePropertiesKHR> modes(mode_count);
        result = enumerate_modes(physical_device, display.display, &mode_count, modes.data());
        if (result != VK_SUCCESS || mode_count == 0)
            continue;

        for (uint32_t plane_index = 0; plane_index < plane_count; ++plane_index) {
            uint32_t supported_count = 0;
            result = supported_displays(physical_device, plane_index, &supported_count, nullptr);
            if (result != VK_SUCCESS || supported_count == 0)
                continue;

            std::vector<VkDisplayKHR> supported(supported_count);
            result = supported_displays(physical_device, plane_index, &supported_count, supported.data());
            if (result != VK_SUCCESS)
                continue;

            bool display_supported = false;
            for (const VkDisplayKHR candidate : supported) {
                if (candidate == display.display) {
                    display_supported = true;
                    break;
                }
            }
            if (!display_supported)
                continue;

            VkDisplayPlaneCapabilitiesKHR capabilities{};
            result = plane_capabilities(physical_device, modes.front().displayMode,
                                        plane_index, &capabilities);
            if (result != VK_SUCCESS)
                continue;

            VkDisplayPlaneAlphaFlagBitsKHR alpha = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
            if ((capabilities.supportedAlpha & alpha) == 0) {
                if ((capabilities.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR) != 0)
                    alpha = VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
                else if ((capabilities.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR) != 0)
                    alpha = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
                else if ((capabilities.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR) != 0)
                    alpha = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
                else
                    continue;
            }

            VkDisplaySurfaceCreateInfoKHR create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
            create_info.displayMode = modes.front().displayMode;
            create_info.planeIndex = plane_index;
            create_info.planeStackIndex = planes[plane_index].currentStackIndex;
            create_info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            create_info.globalAlpha = 1.0f;
            create_info.alphaMode = alpha;
            create_info.imageExtent = modes.front().parameters.visibleRegion;

            VkSurfaceKHR surface = VK_NULL_HANDLE;
            result = create_surface(instance, &create_info, nullptr, &surface);
            if (result != VK_SUCCESS || surface == VK_NULL_HANDLE) {
                log_vulkan_failure("vkCreateDisplayPlaneSurfaceKHR", result);
                continue;
            }

            width = static_cast<int>(create_info.imageExtent.width);
            height = static_cast<int>(create_info.imageExtent.height);
            std::fprintf(stderr, "[Vita3K-PS4] selected RADV display mode %ux%u\n",
                         create_info.imageExtent.width, create_info.imageExtent.height);
            return surface;
        }
    }

    std::fprintf(stderr,
        "[Vita3K-PS4] no display mode/plane can create a surface; check the PS4 RADV VideoOut WSI.\n");
    return VK_NULL_HANDLE;
}

void FrameHost::destroy_surface(VkInstance instance, VkSurfaceKHR surface) const {
    const auto destroy_surface =
        load_instance_function<PFN_vkDestroySurfaceKHR>(instance, "vkDestroySurfaceKHR");
    if (destroy_surface && surface != VK_NULL_HANDLE)
        destroy_surface(instance, surface, nullptr);
}

int FrameHost::drawable_width() const {
    return width;
}

int FrameHost::drawable_height() const {
    return height;
}

std::vector<std::string> FrameHost::font_dirs() const {
    return {"/app0/data/fonts", "/system/fonts"};
}

void *FrameHost::get_proc_address(const char *) const {
    return nullptr;
}

bool FrameHost::make_current() {
    return false;
}

void FrameHost::done_current() {
}

void FrameHost::swap_buffers() {
    // Vulkan presentation is performed by RADV's PS4 swapchain/VideoOut implementation.
}

bool FrameHost::set_vsync(bool) {
    return true;
}

void FrameHost::prepare_for_render_thread() {
}

void FrameHost::destroy_render_context() {
}

} // namespace sdl_frontend
