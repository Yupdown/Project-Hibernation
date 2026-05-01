#include "stdafx.h"
#include "hb_image_resources.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace hb {

namespace {

uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags props) {
	vk::PhysicalDeviceMemoryProperties memProps = physicalDevice.getMemoryProperties();
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
		if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}
	throw std::runtime_error("findMemoryType: no suitable memory type");
}

void submitOneShot(
	vk::Device device,
	vk::Queue queue,
	vk::CommandPool pool,
	const std::function<void(vk::CommandBuffer)>& record)
{
	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.commandPool = pool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;

	vk::CommandBuffer cb = device.allocateCommandBuffers(allocInfo).front();

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cb.begin(beginInfo);
	record(cb);
	cb.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cb;
	queue.submit(submitInfo, nullptr);
	queue.waitIdle();

	device.freeCommandBuffers(pool, cb);
}

} // namespace

LoadedTextureRgba8 uploadRgba8TextureFromFile(
	vk::PhysicalDevice physicalDevice,
	vk::Device device,
	vk::Queue graphicsQueue,
	vk::CommandPool commandPool,
	const std::filesystem::path& imagePath)
{
	const std::string pathUtf8 = imagePath.string();
	int w = 0;
	int h = 0;
	stbi_uc* pixels = stbi_load(pathUtf8.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
	if (!pixels || w <= 0 || h <= 0) {
		const char* reason = stbi_failure_reason();
		throw std::runtime_error(
			std::string("stbi_load failed for ") + pathUtf8 + (reason ? std::string(": ") + reason : ""));
	}

	const uint32_t width = static_cast<uint32_t>(w);
	const uint32_t height = static_cast<uint32_t>(h);
	const vk::DeviceSize imageSize = static_cast<vk::DeviceSize>(width) * height * 4u;

	LoadedTextureRgba8 out{};

	vk::BufferCreateInfo stagingBufInfo{};
	stagingBufInfo.size = imageSize;
	stagingBufInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
	stagingBufInfo.sharingMode = vk::SharingMode::eExclusive;
	vk::UniqueBuffer stagingBuffer = device.createBufferUnique(stagingBufInfo);

	vk::MemoryRequirements stagingReq = device.getBufferMemoryRequirements(*stagingBuffer);
	vk::MemoryAllocateInfo stagingAlloc{};
	stagingAlloc.allocationSize = stagingReq.size;
	stagingAlloc.memoryTypeIndex = findMemoryType(
		physicalDevice,
		stagingReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	vk::UniqueDeviceMemory stagingMemory = device.allocateMemoryUnique(stagingAlloc);
	device.bindBufferMemory(*stagingBuffer, *stagingMemory, 0);

	void* mapped = device.mapMemory(*stagingMemory, 0, imageSize);
	std::memcpy(mapped, pixels, static_cast<size_t>(imageSize));
	device.unmapMemory(*stagingMemory);
	stbi_image_free(pixels);

	vk::ImageCreateInfo imageInfo{};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent = vk::Extent3D{ width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = vk::Format::eR8G8B8A8Unorm;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	out.image = device.createImageUnique(imageInfo);

	vk::MemoryRequirements imgReq = device.getImageMemoryRequirements(*out.image);
	vk::MemoryAllocateInfo imgAlloc{};
	imgAlloc.allocationSize = imgReq.size;
	imgAlloc.memoryTypeIndex = findMemoryType(
		physicalDevice, imgReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
	out.memory = device.allocateMemoryUnique(imgAlloc);
	device.bindImageMemory(*out.image, *out.memory, 0);

	vk::ImageViewCreateInfo viewInfo{};
	viewInfo.image = *out.image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = vk::Format::eR8G8B8A8Unorm;
	viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	out.view = device.createImageViewUnique(viewInfo);

	out.width = width;
	out.height = height;

	submitOneShot(device, graphicsQueue, commandPool, [&](vk::CommandBuffer cb) {
		vk::ImageMemoryBarrier toTransfer{};
		toTransfer.oldLayout = vk::ImageLayout::eUndefined;
		toTransfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
		toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.image = *out.image;
		toTransfer.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		toTransfer.subresourceRange.baseMipLevel = 0;
		toTransfer.subresourceRange.levelCount = 1;
		toTransfer.subresourceRange.baseArrayLayer = 0;
		toTransfer.subresourceRange.layerCount = 1;
		toTransfer.srcAccessMask = {};
		toTransfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		cb.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{},
			nullptr,
			nullptr,
			toTransfer);

		vk::BufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = vk::Offset3D{ 0, 0, 0 };
		region.imageExtent = vk::Extent3D{ width, height, 1 };

		cb.copyBufferToImage(
			*stagingBuffer,
			*out.image,
			vk::ImageLayout::eTransferDstOptimal,
			region);

		vk::ImageMemoryBarrier toShader{};
		toShader.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		toShader.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toShader.image = *out.image;
		toShader.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		toShader.subresourceRange.baseMipLevel = 0;
		toShader.subresourceRange.levelCount = 1;
		toShader.subresourceRange.baseArrayLayer = 0;
		toShader.subresourceRange.layerCount = 1;
		toShader.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		toShader.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		cb.pipelineBarrier(
			vk::PipelineStageFlagBits::eTransfer,
			vk::PipelineStageFlagBits::eFragmentShader,
			{},
			nullptr,
			nullptr,
			toShader);
	});

	return out;
}

} // namespace hb
