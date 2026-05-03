#pragma once

#include "hb_frame_stats.h"

#define HB_FRAME_STATS_MACRO_CONCAT_INNER(a, b) a##b
#define HB_FRAME_STATS_MACRO_CONCAT(a, b) HB_FRAME_STATS_MACRO_CONCAT_INNER(a, b)

// RAII: measures elapsed time to end of scope; adds ms to g_frameStats pending bucket (string key).
#define HB_FRAME_STATS_SCOPE(sectionNameLiteral) \
	const HBFrameStats::ScopedSection HB_FRAME_STATS_MACRO_CONCAT(_hb_frame_stats_scope_, __LINE__) { g_frameStats, sectionNameLiteral }
