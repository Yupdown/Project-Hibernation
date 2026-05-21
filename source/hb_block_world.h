#pragma once

#include <cstdint>
#include <vector>

/** Matches `QuadPushConstants` in shader.vert / shader.frag (std430 push_constant layout). */
struct BlockWorldPushConstants {
	uint32_t drawMode;
	float timeSeconds;
	uint32_t flipUvU;
	uint32_t flipUvV;
	alignas(16) glm::mat4 mvp;
};

struct BlockWorldOffscreenRenderContext {
	vk::CommandBuffer commandBuffer;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline fillPipeline;
	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
	vk::DescriptorSet textureDescriptorSet;
	vk::Extent2D offscreenExtent;
};

struct BlockWorldWireframeRenderContext {
	vk::CommandBuffer commandBuffer;
	vk::PipelineLayout pipelineLayout;
	vk::Pipeline wireframePipeline;
	vk::Buffer vertexBuffer;
	vk::Buffer indexBuffer;
	vk::DescriptorSet textureDescriptorSet;
	vk::Extent2D swapChainExtent;
};

/** CPU-sorted tile block map drawn without depth testing. */
class BlockWorld {
public:
	static constexpr uint32_t MapSize = 10u;
	static constexpr uint32_t QuadIndexCount = 21u;
	static constexpr uint32_t DrawModeTextured = 0u;
	static constexpr uint32_t DrawModeWireframeOverlay = 1u;

	void update(
		float timeSeconds,
		uint32_t swapChainWidth,
		uint32_t swapChainHeight,
		uint32_t textureWidth,
		uint32_t textureHeight,
		uint32_t offscreenScale,
		bool flipUvU,
		bool flipUvV);

	void renderOffscreen(const BlockWorldOffscreenRenderContext& ctx) const;
	void renderWireframe(const BlockWorldWireframeRenderContext& ctx) const;

private:
	struct TileDrawOrder {
		uint32_t x;
		uint32_t z;
		float viewDepth;
	};

	static bool flipUvUForRotationCycle(float cycleDeg);
	static glm::vec3 tileCenter(uint32_t x, uint32_t z);
	glm::mat4 tileModelMatrix(uint32_t x, uint32_t z) const;

	float m_degrees = 0.f;
	glm::mat4 m_projView{ 1.f };
	BlockWorldPushConstants m_texturedPush{};
	std::vector<TileDrawOrder> m_sortedTiles;
};
