#include "stdafx.h"

#include "hb_block_world.h"

#include <cstddef>

static_assert(sizeof(BlockWorldPushConstants) == 80);
static_assert(offsetof(BlockWorldPushConstants, mvp) == 16);

bool BlockWorld::flipUvUForRotationCycle(float cycleDeg) {
	return (cycleDeg >= 45.0f && cycleDeg < 135.0f) || (cycleDeg >= 225.0f && cycleDeg < 315.0f);
}

glm::vec3 BlockWorld::tileCenter(uint32_t x, uint32_t z) {
	return glm::vec3(
		static_cast<float>(x) + static_cast<float>(z),
		0.f,
		static_cast<float>(x) - static_cast<float>(z));
}

glm::mat4 BlockWorld::tileModelMatrix(uint32_t x, uint32_t z) const {
	const glm::mat4 translation = glm::translate(glm::mat4(1.f), tileCenter(x, z));
	const glm::mat4 rotation = glm::rotate(
		glm::mat4(1.f),
		glm::radians(glm::floor((45.0f - m_degrees) / 90.0f) * 90.0f),
		glm::vec3(0.f, 1.f, 0.f));
	return translation * rotation;
}

void BlockWorld::update(
	float timeSeconds,
	uint32_t swapChainWidth,
	uint32_t swapChainHeight,
	uint32_t textureWidth,
	uint32_t textureHeight,
	uint32_t offscreenScale,
	bool flipUvU,
	bool flipUvV) {
	m_degrees = glm::mod(timeSeconds * 180.0f, 360.0f);
	const bool flipUvUFromRotation = flipUvUForRotationCycle(m_degrees);

	glm::mat4 view = glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, -0.5f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	view = view * glm::rotate(glm::mat4(1.f), glm::radians(m_degrees), glm::vec3(0.f, 1.f, 0.f));

	const float winW = static_cast<float>(std::max(1u, swapChainWidth));
	const float winH = static_cast<float>(std::max(1u, swapChainHeight));
	const float a_win = winW / winH;
	const float imgW = static_cast<float>(std::max(1u, textureWidth));
	const float imgH = static_cast<float>(std::max(1u, textureHeight));
	const float a_img = imgW / imgH;
	const float sx = a_img / a_win / static_cast<float>(offscreenScale);
	const float sy = 1.0f / static_cast<float>(offscreenScale);
	const glm::mat4 projection = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
	m_projView = projection * view;

	m_texturedPush = {};
	m_texturedPush.drawMode = DrawModeTextured;
	m_texturedPush.timeSeconds = timeSeconds;
	m_texturedPush.flipUvU = (flipUvU || flipUvUFromRotation) ? 1u : 0u;
	m_texturedPush.flipUvV = flipUvV ? 1u : 0u;

	m_sortedTiles.clear();
	m_sortedTiles.reserve(MapSize * MapSize);
	for (uint32_t z = 0; z < MapSize; ++z) {
		for (uint32_t x = 0; x < MapSize; ++x) {
			const glm::vec3 center = tileCenter(x, z);
			m_sortedTiles.push_back({
				x,
				z,
				(view * glm::vec4(center, 1.f)).z,
			});
		}
	}
	std::sort(
		m_sortedTiles.begin(),
		m_sortedTiles.end(),
		[](const TileDrawOrder& a, const TileDrawOrder& b) { return a.viewDepth < b.viewDepth; });
}

void BlockWorld::renderOffscreen(const BlockWorldOffscreenRenderContext& ctx) const {
	ctx.commandBuffer.setViewport(0, vk::Viewport{
		0.0f,
		static_cast<float>(ctx.offscreenExtent.height),
		static_cast<float>(ctx.offscreenExtent.width),
		-static_cast<float>(ctx.offscreenExtent.height),
		0.0f,
		1.0f
		});
	ctx.commandBuffer.setScissor(0, vk::Rect2D{
		vk::Offset2D{ 0, 0 },
		ctx.offscreenExtent
		});

	vk::DeviceSize vbOffset = 0;
	ctx.commandBuffer.bindVertexBuffers(0, 1, &ctx.vertexBuffer, &vbOffset);
	ctx.commandBuffer.bindIndexBuffer(ctx.indexBuffer, 0, vk::IndexType::eUint16);

	vk::DescriptorSet texSets[] = { ctx.textureDescriptorSet };
	ctx.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		ctx.pipelineLayout,
		0,
		1,
		texSets,
		0,
		nullptr);

	ctx.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, ctx.fillPipeline);

	BlockWorldPushConstants push = m_texturedPush;
	for (const TileDrawOrder& tile : m_sortedTiles) {
		const glm::mat4 model = tileModelMatrix(tile.x, tile.z);
		push.mvp = m_projView * model;
		ctx.commandBuffer.pushConstants(
			ctx.pipelineLayout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			sizeof(BlockWorldPushConstants),
			&push);
		ctx.commandBuffer.drawIndexed(QuadIndexCount, 1, 0, 0, 0);
	}
}

void BlockWorld::renderWireframe(const BlockWorldWireframeRenderContext& ctx) const {
	ctx.commandBuffer.setViewport(0, vk::Viewport{
		0.0f,
		static_cast<float>(ctx.swapChainExtent.height),
		static_cast<float>(ctx.swapChainExtent.width),
		-static_cast<float>(ctx.swapChainExtent.height),
		0.0f,
		1.0f
		});
	ctx.commandBuffer.setScissor(0, vk::Rect2D{
		vk::Offset2D{ 0, 0 },
		ctx.swapChainExtent
		});

	vk::DeviceSize vbOffset = 0;
	ctx.commandBuffer.bindVertexBuffers(0, 1, &ctx.vertexBuffer, &vbOffset);
	ctx.commandBuffer.bindIndexBuffer(ctx.indexBuffer, 0, vk::IndexType::eUint16);

	vk::DescriptorSet texSets[] = { ctx.textureDescriptorSet };
	ctx.commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		ctx.pipelineLayout,
		0,
		1,
		texSets,
		0,
		nullptr);

	ctx.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, ctx.wireframePipeline);

	BlockWorldPushConstants wirePush = m_texturedPush;
	wirePush.drawMode = DrawModeWireframeOverlay;
	for (const TileDrawOrder& tile : m_sortedTiles) {
		const glm::mat4 model = tileModelMatrix(tile.x, tile.z);
		wirePush.mvp = m_projView * model;
		ctx.commandBuffer.pushConstants(
			ctx.pipelineLayout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			0,
			sizeof(BlockWorldPushConstants),
			&wirePush);
		ctx.commandBuffer.drawIndexed(QuadIndexCount, 1, 0, 0, 0);
	}
}
