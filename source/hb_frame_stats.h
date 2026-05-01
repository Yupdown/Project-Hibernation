#pragma once

class HBFrameStats {
public:
	static constexpr size_t FRAME_HISTORY_SIZE = 100;
	static constexpr double GRAPH_UPDATE_INTERVAL = 1.0 / 120.0;  // 60 FPS interval
	static constexpr double FPS_UPDATE_INTERVAL = 1.0 / 20.0;    // 20 times per second

	struct FrameTimeStats {
		float avg = 0.0f;
		float min = 0.0f;
		float max = 0.0f;
	};

public:
	HBFrameStats();

	void update();
	void renderImGui(bool& vsync, bool& recreateSwapchain, bool& quadWireframe);

	// Getters
	double getFPS() const { return m_fps; }
	float getCurrentFrameTime() const { return m_currentFrameTime; }
	float getMinFrameTime() const { return m_minFrameTime; }
	float getMaxFrameTime() const { return m_maxFrameTime; }
	float getAvgFrameTime() const { return m_avgFrameTime; }
	float getFrameAxisLimit() const { return m_frameAxisLimit; }
	uint64_t getTimelineValue() const { return m_timelineValue; }

	// Setters
	void setTimelineValue(uint64_t value) { m_timelineValue = value; }

private:
	// Frame rate measurement
	double m_lastTime = 0.0;
	int m_frameCount = 0;
	double m_fps = 0.0;

	// Frame time tracking
	double m_lastFrameTime = 0.0;
	double m_graphUpdateTimer = 0.0;

	float m_currentFrameTime = 0.0f;
	float m_minFrameTime = FLT_MAX;
	float m_maxFrameTime = 0.0f;
	float m_avgFrameTime = 0.0f;
	float m_frameAxisLimit = 0.0f;

	// Frame time histogram data
	std::array<FrameTimeStats, FRAME_HISTORY_SIZE> m_frameTimeHistory;

	// Accumulator for current graph point
	std::vector<float> m_frameTimeAccumulator;

	// Timeline synchronization
	uint64_t m_timelineValue = 0;
};
