#include "stdafx.h"
#include "hb_frame_stats.h"

HBFrameStats g_frameStats;

HBFrameStats::HBFrameStats() {
	m_frameTimeSamples.reserve(4096);
	m_plotScratchX.reserve(4096);

	const double t = glfwGetTime();
	m_fpsIntervalStart = t;
	m_lastFrameTime = t;
}

void HBFrameStats::newFrame() {
	const double currentTime = glfwGetTime();

	m_currentFrameTime = static_cast<float>(currentTime - m_lastFrameTime);
	m_lastFrameTime = currentTime;

	const float frameTimeMs = m_currentFrameTime * 1000.0f;
	std::vector<std::pair<std::string_view, double>> sectionMs;
	double cumulativeMs = 0.0;
	for (const auto& [sectionKey, ms] : m_stagingTimeSamples) {
		sectionMs.emplace_back(sectionKey, ms);
		cumulativeMs += ms;
	}
	sectionMs.emplace_back("Unspecified", std::max(0.0, static_cast<double>(frameTimeMs) - cumulativeMs));
	if (m_frameTimeSamples.size() >= SAMPLE_COUNT) {
		compressSamples();
	}
	m_frameTimeSamples.emplace_back(FrameTimeSample{ currentTime, frameTimeMs, std::move(sectionMs) });
	m_stagingTimeSamples.clear();

	updateRollingStats();

	m_fpsFramesInInterval++;
	if (currentTime - m_fpsIntervalStart >= FPS_UPDATE_INTERVAL) {
		m_fps = static_cast<double>(m_fpsFramesInInterval) / (currentTime - m_fpsIntervalStart);
		m_fpsFramesInInterval = 0;
		m_fpsIntervalStart = currentTime;
	}
}

void HBFrameStats::addSectionMs(const char* sectionName, double ms) {
	m_stagingTimeSamples[sectionName] += ms;
}

void HBFrameStats::updateRollingStats() {
	const size_t n = m_frameTimeSamples.size();
	const size_t window = std::min(n, STATS_FRAME_WINDOW);
	const size_t start = n - window;

	double sumMs = 0.0;
	float winMin = std::numeric_limits<float>::max();
	float winMax = 0.0f;
	for (size_t i = start; i < n; ++i) {
		const float v = m_frameTimeSamples[i].frameTimeMs;
		sumMs += static_cast<double>(v);
		winMin = std::min(winMin, v);
		winMax = std::max(winMax, v);
	}
	m_statsLastN.avgMs = static_cast<float>(sumMs / static_cast<double>(window));
	m_statsLastN.minMs = winMin;
	m_statsLastN.maxMs = winMax;
}

void HBFrameStats::compressSamples()
{
	for (size_t index = 0; index < SAMPLE_THRESHOLD; ++index) {
		const FrameTimeSample& lhs = m_frameTimeSamples[index * 2];
		const FrameTimeSample& rhs = m_frameTimeSamples[index * 2 + 1];
		std::vector<std::pair<std::string_view, double>> mergedSectionMs = lhs.sectionMs;
		for (const auto& [sectionKey, ms] : rhs.sectionMs) {
			const auto it = std::find_if(mergedSectionMs.begin(), mergedSectionMs.end(),
				[&sectionKey](const std::pair<std::string_view, double>& p) {
					return p.first == sectionKey;
				});
			if (it != mergedSectionMs.end()) {
				it->second = (it->second + ms) / 2.0;
			}
			else {
				mergedSectionMs.emplace_back(sectionKey, ms);
			}
		}
		m_frameTimeSamples[index] = FrameTimeSample{
			rhs.timeSec,
			(lhs.frameTimeMs + rhs.frameTimeMs) / 2,
			std::move(mergedSectionMs)
		};
	}
	for (size_t index = 0; index < SAMPLE_THRESHOLD; ++index) {
		m_frameTimeSamples[SAMPLE_THRESHOLD + index] = m_frameTimeSamples[SAMPLE_THRESHOLD * 2 + index];
	}
	m_frameTimeSamples.resize(SAMPLE_THRESHOLD * 2);
}

void HBFrameStats::renderImGui(bool& vsync, bool& recreateSwapchain, bool& quadWireframe) {
	ImGui::Begin("Frame Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::Checkbox("VSync", &vsync)) {
		recreateSwapchain = true;
	}
	ImGui::Checkbox("Quad wireframe", &quadWireframe);

	ImGui::Text("FPS: %.1f", m_fps);
	ImGui::Text("Frame Time: %.3f ms", m_currentFrameTime * 1000.0f);
	ImGui::Separator();

	if (m_frameTimeSamples.empty()) {
		ImGui::Text("Min: -- | Max: -- | Avg: --");
	} else {
		ImGui::Text("Min: %.3f ms | Max: %.3f ms | Avg: %.3f ms",
			m_statsLastN.minMs, m_statsLastN.maxMs, m_statsLastN.avgMs);
	}
	ImGui::Text("Samples: %zu", m_frameTimeSamples.size());
	ImGui::Text("Timeline Value: %llu", static_cast<unsigned long long>(m_timelineValue));

	ImGui::Separator();

	ImGui::SliderFloat("Plot history (s)", &m_plotHistoryWindowSec, 0.1f, 100.0f, "%.1f", ImGuiSliderFlags_Logarithmic);

	const double plotTimeNow = m_lastFrameTime;
	const double plotTimeMin = plotTimeNow - static_cast<double>(m_plotHistoryWindowSec);

	if (!m_frameTimeSamples.empty()) {
		// Find the range of samples to plot (those with timeSec in [plotTimeMin, plotTimeNow])
		const auto& samples = m_frameTimeSamples;
		auto windowIt = std::lower_bound(samples.begin(), samples.end(), plotTimeMin,
			[](const FrameTimeSample& s, double t) { return s.timeSec < t; });
		if (windowIt != samples.begin()) {
			windowIt = std::prev(windowIt);
		}
		auto windowEnd = samples.end();
		for (auto j = windowIt; j != samples.end(); ++j) {
			if (j->timeSec > plotTimeNow) {
				windowEnd = j;
				break;
			}
		}

		const FrameTimeSample& latestSample = m_frameTimeSamples.back();
		const std::size_t layerCount = latestSample.sectionMs.size();

		m_plotScratchX.clear();
		m_plotScratchYByLayer.assign(layerCount, {});
		for (std::vector<float>& layer : m_plotScratchYByLayer) {
			layer.reserve(4096);
		}

		for (auto it = windowIt; it != windowEnd; ++it) {
			m_plotScratchX.push_back(static_cast<float>(it->timeSec));
			double cumulativeMs = 0.0;
			std::size_t layerIndex = 0;
			for (const auto& [sectionKey, ms] : it->sectionMs) {
				cumulativeMs += ms;
				m_plotScratchYByLayer[layerIndex].push_back(static_cast<float>(cumulativeMs));
				++layerIndex;
			}
		}

		const int plotCount = static_cast<int>(m_plotScratchX.size());

		float targetYAxisMax = 1.0f;
		if (!m_plotScratchYByLayer.empty() && !m_plotScratchYByLayer.back().empty()) {
			const auto& topLayer = m_plotScratchYByLayer.back();
			const float segMax = *std::max_element(topLayer.begin(), topLayer.end());
			targetYAxisMax = segMax * 1.25f;
			if (targetYAxisMax <= 0.0f) {
				targetYAxisMax = 1.0f;
			}
		}

		const float dt = ImGui::GetIO().DeltaTime;
		constexpr float kPlotYAxisLerpRate = 16.0f;
		const float yAxisLerpAlpha = std::clamp(dt * kPlotYAxisLerpRate, 0.0f, 1.0f);
		m_plotYAxisMaxSmoothed = std::lerp(m_plotYAxisMaxSmoothed, targetYAxisMax, yAxisLerpAlpha);

		if (ImPlot::BeginPlot("Frame time", ImVec2(-1, 300))) {
			ImPlot::SetupAxes("Time (s)", "Time (ms)");
			ImPlot::SetupAxisFormat(ImAxis_X1, "%.1f");
			ImPlot::SetupAxisLimits(ImAxis_X1, plotTimeMin, plotTimeNow, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, static_cast<double>(m_plotYAxisMaxSmoothed), ImGuiCond_Always);

			if (plotCount > 0 && layerCount > 0) {
				const float* const xs = m_plotScratchX.data();

				std::size_t layerIndex = 0;
				for (const auto& [sectionKey, unused] : latestSample.sectionMs) {
					static_cast<void>(unused);
					const float* const yCurr = m_plotScratchYByLayer[layerIndex].data();
					if (layerIndex == 0) {
						ImPlot::PlotShaded(sectionKey.data(), xs, yCurr, plotCount, 0.0);
					}
					else {
						const float* const yBelow = m_plotScratchYByLayer[layerIndex - 1].data();
						ImPlot::PlotShaded(sectionKey.data(), xs, yBelow, yCurr, plotCount);
					}
					++layerIndex;
				}
			}

			ImPlot::EndPlot();
		}

		ImGui::Separator();

		// Pie chart of latest frame breakdown
		std::vector<const char*> kPieLabels;
		std::vector<double> pieValsD;
		double pieSumD = 0.0;

		for (const auto& [sectionKey, ms] : latestSample.sectionMs) {
			kPieLabels.push_back(sectionKey.data());
			pieValsD.push_back(ms);
			pieSumD += ms;
		}

		constexpr double kPieEpsilon = 1e-9;
		if (pieSumD > kPieEpsilon) {
			if (ImPlot::BeginPlot(
				"Frame breakdown",
				ImVec2(-1, 0),
				ImPlotFlags_Equal | ImPlotFlags_NoMouseText)) {
				ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
				ImPlot::SetupAxesLimits(0.0, 1.0, 0.0, 1.0);
				ImPlot::PlotPieChart(
					kPieLabels.data(),
					pieValsD.data(),
					static_cast<int>(pieValsD.size()),
					0.5,
					0.5,
					0.4,
					"%.3f ms",
					90.0,
					{
						ImPlotProp_Flags,
						ImPlotPieChartFlags_Normalize,
					});
				ImPlot::EndPlot();
			}
		}
	}

	ImGui::End();
}
