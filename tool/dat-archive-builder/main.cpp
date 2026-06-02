#include "core/FileSystemDATArchive.hpp"

#include <algorithm>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>
#include <vector>

using std::string_view_literals::operator ""sv;

namespace {

std::u8string_view getUtf8StringView(std::string_view const& s) {
	return { reinterpret_cast<char8_t const*>(s.data()), s.size() };
}

std::string_view getStringView(std::u8string_view const& s) {
	return { reinterpret_cast<char const*>(s.data()), s.size() };
}

void printUsage() {
	std::println("usage: dat-archive-builder --input <directory> --output <archive.dat>");
}

} // namespace

int main(int argc, char** argv) {
	std::string input;
	std::string output;

	for (int i = 1; i < argc; ++i) {
		std::string_view const arg(argv[i]);
		if (arg == "-h"sv || arg == "--help"sv) {
			printUsage();
			return 0;
		}
		if (arg == "-i"sv || arg == "--input"sv) {
			if (++i >= argc) {
				std::println("error: missing input path");
				return 1;
			}
			input = argv[i];
			continue;
		}
		if (arg == "-o"sv || arg == "--output"sv) {
			if (++i >= argc) {
				std::println("error: missing output path");
				return 1;
			}
			output = argv[i];
			continue;
		}
		std::println("error: unknown argument '{}'", arg);
		printUsage();
		return 1;
	}

	if (input.empty() || output.empty()) {
		printUsage();
		return 1;
	}

	std::error_code ec;
	std::filesystem::path const inputPath(getUtf8StringView(input));
	if (!std::filesystem::is_directory(inputPath, ec)) {
		std::println("error: '{}' is not a directory", input);
		return 1;
	}

	std::filesystem::path const outputPath(getUtf8StringView(output));
	std::filesystem::path const outputDir = outputPath.parent_path();
	if (!outputDir.empty() && !std::filesystem::exists(outputDir, ec)) {
		if (!std::filesystem::create_directories(outputDir, ec)) {
			auto const path = outputDir.generic_u8string();
			std::println("error: create directory '{}' failed", getStringView(path));
			return 1;
		}
	}

	std::vector<std::string> files;
	std::filesystem::recursive_directory_iterator it(inputPath, ec);
	if (ec) {
		std::println("error: enumerate '{}' failed", input);
		return 1;
	}
	std::filesystem::recursive_directory_iterator end;
	while (it != end) {
		auto const& entry = *it;
		bool const isFile = entry.is_regular_file(ec);
		if (ec) {
			std::println("error: enumerate '{}' failed", input);
			return 1;
		}
		if (isFile) {
			auto const path = entry.path().lexically_relative(inputPath).lexically_normal().generic_u8string();
			if (!path.empty() && path != u8"."sv) {
				files.emplace_back(getStringView(path));
			}
		}

		it.increment(ec);
		if (ec) {
			std::println("error: enumerate '{}' failed", input);
			return 1;
		}
	}
	std::ranges::sort(files);

	core::DATArchiveCreator creator;
	for (auto const& file : files) {
		creator.addFile(file);
	}
	if (!creator.create(input, output)) {
		return 1;
	}

	std::println("created '{}' with {} file(s)", output, files.size());
	return 0;
}
