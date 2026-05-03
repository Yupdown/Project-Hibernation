#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class HBFrameStats {
public:
	static constexpr double FPS_UPDATE_INTERVAL = 1.0 / 20.0;
	static constexpr std::size_t STATS_FRAME_WINDOW = 100;

	struct FrameTimeSample {
		double timeSec = 0.0;
		float frameTimeMs = 0.0f;
		std::vector<std::pair<std::string_view, double>> sectionMs;
	};

	HBFrameStats();

	void newFrame();
	void renderImGui(bool& vsync, bool& recreateSwapchain, bool& quadWireframe);

	void setTimelineValue(std::uint64_t value) { m_timelineValue = value; }

	void addSectionMs(const char* sectionName, double ms);

	struct ScopedSection {
		HBFrameStats* stats = nullptr;
		const char* sectionName = nullptr;
		std::chrono::steady_clock::time_point t0{};

		ScopedSection(HBFrameStats& s, const char* name)
			: stats(&s), sectionName(name), t0(std::chrono::steady_clock::now()) {}

		ScopedSection(const ScopedSection&) = delete;
		ScopedSection& operator=(const ScopedSection&) = delete;

		~ScopedSection() {
			if (!stats || !sectionName) {
				return;
			}
			const double ms = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - t0).count();
			stats->addSectionMs(sectionName, ms);
		}
	};

private:
	void updateRollingStats();

	// FPS averaged over FPS_UPDATE_INTERVAL
	double m_fpsIntervalStart = 0.0;
	int m_fpsFramesInInterval = 0;
	double m_fps = 0.0;

	double m_lastFrameTime = 0.0;
	float m_currentFrameTime = 0.0f;

	// Last STATS_FRAME_WINDOW samples (updated each frame)
	struct {
		float minMs = std::numeric_limits<float>::max();
		float maxMs = 0.0f;
		float avgMs = 0.0f;
	} m_statsLastN;

	std::vector<FrameTimeSample> m_frameTimeSamples;
	std::map<std::string_view, double> m_stagingTimeSamples{};

	float m_plotHistoryWindowSec = 10.0f;
	float m_plotYAxisMaxSmoothed = 1.0f;

	std::vector<float> m_plotScratchX;
	std::vector<std::vector<float>> m_plotScratchYByLayer;
	std::uint64_t m_timelineValue = 0;
};

// Single-threaded game loop: one global stats instance (see hb_frame_stats.cpp).
extern HBFrameStats g_frameStats;
