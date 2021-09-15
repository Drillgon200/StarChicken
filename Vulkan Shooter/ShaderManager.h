#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

std::vector<char> readFile(const std::string& fileName);
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);