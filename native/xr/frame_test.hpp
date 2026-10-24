#pragma once
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <string>
#include <vector>
std::string d4vr_native_frame_test(XrInstance instance, XrSystemId system, VkInstance vk_instance,
                                 VkPhysicalDevice physical, const std::vector<XrViewConfigurationView>& views,
                                 const char* vertex_path, const char* fragment_path, bool& success);
