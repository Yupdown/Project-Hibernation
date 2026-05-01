#pragma once

#include <filesystem>
#include <vulkan/vulkan.hpp>

namespace hb {

struct LoadedTextureRgba8 {
	vk::UniqueImage image;
	vk::UniqueDeviceMemory memory;
	vk::UniqueImageView view;
	uint32_t width = 0;
	uint32_t height = 0;
};

/** Loads an uncompressed RGBA image via stb_image and uploads to a mip-level-1 R8G8B8A8_SRGB optimal image. */
LoadedTextureRgba8 uploadRgba8TextureFromFile(
	vk::PhysicalDevice physicalDevice,
	vk::Device device,
	vk::Queue graphicsQueue,
	vk::CommandPool commandPool,
	const std::filesystem::path& imagePath);

} // namespace hb
