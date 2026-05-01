#include "stdafx.h"
#include "hb_frame_stats.h"

HBFrameStats::HBFrameStats() {
	// Initialize frame time history
	for (auto& stats : m_frameTimeHistory) {
		stats.avg = 0.0f;
		stats.min = 0.0f;
		stats.max = 0.0f;
	}

	m_frameTimeAccumulator.reserve(100);
	m_lastTime = glfwGetTime();
	m_lastFrameTime = m_lastTime;
}

void HBFrameStats::update() {
	double currentTime = glfwGetTime();

	// Calculate frame time
	m_currentFrameTime = static_cast<float>((currentTime - m_lastFrameTime));
	m_lastFrameTime = currentTime;

	// Add to accumulator
	m_frameTimeAccumulator.push_back(m_currentFrameTime * 1000.0f);

	// Check if it's time to update graph (1/60 second interval or if frame time exceeds the interval)
	m_graphUpdateTimer += m_currentFrameTime;
	if (m_graphUpdateTimer >= GRAPH_UPDATE_INTERVAL) {
		// Calculate statistics from accumulated frame times
		if (!m_frameTimeAccumulator.empty()) {
			float sum = 0.0f;
			float minTime = FLT_MAX;
			float maxTime = 0.0f;

			for (float frameTime : m_frameTimeAccumulator) {
				sum += frameTime;
				minTime = std::min(minTime, frameTime);
				maxTime = std::max(maxTime, frameTime);
			}

			float avgTime = sum / m_frameTimeAccumulator.size();

			std::rotate(m_frameTimeHistory.begin(), m_frameTimeHistory.begin() + 1, m_frameTimeHistory.end());

			// Add to history (shift left)
			m_frameTimeHistory.back().avg = avgTime;
			m_frameTimeHistory.back().min = minTime;
			m_frameTimeHistory.back().max = maxTime;

			// Clear accumulator
			m_frameTimeAccumulator.clear();
		}

		m_graphUpdateTimer = std::fmod(m_graphUpdateTimer, GRAPH_UPDATE_INTERVAL);
	}

	// Calculate overall statistics from history
	float sum = 0.0f;
	int validCount = 0;
	m_minFrameTime = FLT_MAX;
	m_maxFrameTime = 0.0f;

	for (const auto& stats : m_frameTimeHistory) {
		if (stats.avg > 0.0f) {
			sum += stats.avg;
			validCount++;
			m_minFrameTime = std::min(m_minFrameTime, stats.min);
			m_maxFrameTime = std::max(m_maxFrameTime, stats.max);
		}
	}

	if (validCount > 0) {
		m_avgFrameTime = sum / validCount;
	}

	// Update FPS counter (20 times per second)
	m_frameCount++;
	if (currentTime - m_lastTime >= FPS_UPDATE_INTERVAL) {
		m_fps = m_frameCount / (currentTime - m_lastTime);

		m_frameCount = 0;
		m_lastTime = currentTime;
	}

	m_frameAxisLimit = std::lerp(m_frameAxisLimit, m_maxFrameTime * 1.2f, m_currentFrameTime * 16.0f);
}

void HBFrameStats::renderImGui(bool& vsync, bool& recreateSwapchain, bool& quadWireframe) {
	ImGui::Begin("Frame Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::Checkbox("VSync", &vsync)) {
		recreateSwapchain = true;
	}
	ImGui::Checkbox("Quad wireframe", &quadWireframe);

	ImGui::Text("FPS: %.1f", m_fps);
	ImGui::Text("Frame Time: %.3f ms", m_currentFrameTime * 1000.0f);
	ImGui::Text("Min: %.3f ms | Max: %.3f ms | Avg: %.3f ms",
		m_minFrameTime, m_maxFrameTime, m_avgFrameTime);
	ImGui::Text("Timeline Value: %llu", m_timelineValue);

	ImGui::Separator();

	// Prepare data for plotting
	std::array<float, FRAME_HISTORY_SIZE> avgData;
	std::array<float, FRAME_HISTORY_SIZE> minData;
	std::array<float, FRAME_HISTORY_SIZE> maxData;

	for (size_t i = 0; i < FRAME_HISTORY_SIZE; ++i) {
		avgData[i] = m_frameTimeHistory[i].avg;
		minData[i] = m_frameTimeHistory[i].min;
		maxData[i] = m_frameTimeHistory[i].max;
	}

	if (ImPlot::BeginPlot("Frame Time Histogram", ImVec2(-1, 300))) {
		ImPlot::SetupAxes("Frame", "Time (ms)");
		ImPlot::SetupAxisTicks(ImAxis_X1, nullptr, 0);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, FRAME_HISTORY_SIZE, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0, m_frameAxisLimit, ImGuiCond_Always);

		// Plot frame time data
		ImPlot::PlotBars("Max", maxData.data(), FRAME_HISTORY_SIZE, 1.0, 0.0, { ImPlotProp_FillColor, ImVec4(0x3c / 255.0f, 0xae / 255.0f, 0xa3 / 255.0f, 1.0f), ImPlotProp_LineColor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f) });
		ImPlot::PlotBars("Avg", avgData.data(), FRAME_HISTORY_SIZE, 1.0, 0.0, { ImPlotProp_FillColor, ImVec4(0x20 / 255.0f, 0x63 / 255.0f, 0x9b / 255.0f, 1.0f), ImPlotProp_LineColor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f) });
		ImPlot::PlotBars("Min", minData.data(), FRAME_HISTORY_SIZE, 1.0, 0.0, { ImPlotProp_FillColor, ImVec4(0x17 / 255.0f, 0x3f / 255.0f, 0x5f / 255.0f, 1.0f), ImPlotProp_LineColor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f) });

		ImPlot::EndPlot();
	}

	ImGui::End();
}
