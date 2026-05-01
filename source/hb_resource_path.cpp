#include "stdafx.h"
#include "hb_resource_path.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace hb {

namespace {

std::filesystem::path canonicalOrLexical(const std::filesystem::path& p) {
	std::error_code ec;
	std::filesystem::path c = std::filesystem::weakly_canonical(p, ec);
	if (!ec) {
		return c;
	}
	return p.lexically_normal();
}

} // namespace

std::filesystem::path executableDirectory() {
#ifdef _WIN32
	wchar_t buf[MAX_PATH];
	DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) {
		return canonicalOrLexical(std::filesystem::current_path());
	}
	return canonicalOrLexical(std::filesystem::path(buf).parent_path());
#elif defined(__APPLE__)
	char buf[PATH_MAX];
	uint32_t size = sizeof(buf);
	if (_NSGetExecutablePath(buf, &size) != 0) {
		return canonicalOrLexical(std::filesystem::current_path());
	}
	return canonicalOrLexical(std::filesystem::path(buf).parent_path());
#else
	char buf[PATH_MAX];
	const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (len == -1) {
		return canonicalOrLexical(std::filesystem::current_path());
	}
	buf[len] = '\0';
	return canonicalOrLexical(std::filesystem::path(buf).parent_path());
#endif
}

const std::filesystem::path& resourceRootDirectory() {
	static const std::filesystem::path root = [] {
		std::filesystem::path dir = executableDirectory();
		for (int i = 0; i < 12; ++i) {
			std::filesystem::path candidate = dir / "resource";
			std::error_code ec;
			if (std::filesystem::is_directory(candidate, ec)) {
				return canonicalOrLexical(candidate);
			}
			std::filesystem::path parent = dir.parent_path();
			if (parent == dir) {
				break;
			}
			dir = std::move(parent);
		}
		return canonicalOrLexical(executableDirectory() / "resource");
	}();
	return root;
}

} // namespace hb
