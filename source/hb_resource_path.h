#pragma once

#include <filesystem>

namespace hb {

/** Absolute path to the directory containing the running executable. */
std::filesystem::path executableDirectory();

/**
 * Asset root: a directory named `resource`.
 * Walks up from the executable directory (limited depth) so layouts like
 * `repo/build/Debug/app.exe` with `repo/resource/` resolve correctly.
 * If none is found, returns `executableDirectory() / "resource"`.
 */
const std::filesystem::path& resourceRootDirectory();

} // namespace hb
