#include <string_view>
#include <string>
#include <filesystem>
#include <fstream>
#include <print>
#include <iostream>
#include <vector>
#include "core/FileSystemWindows.hpp"
#include "core/FileSystem.hpp"
#include "core/FileSystemDATArchive.hpp"
#include "core/SmartReference.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "gtest/gtest.h"

using std::string_view_literals::operator ""sv;

namespace {
	std::string pathToUtf8(std::filesystem::path const& path) {
		auto const path_u8 = path.lexically_normal().generic_u8string();
		return { reinterpret_cast<char const*>(path_u8.data()), path_u8.size() };
	}

	void writeBinaryFile(std::filesystem::path const& path, std::string_view const& content) {
		std::filesystem::create_directories(path.parent_path());
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		ASSERT_TRUE(file.is_open());
		file.write(content.data(), static_cast<std::streamsize>(content.size()));
		ASSERT_TRUE(file.good());
	}

	std::string readString(core::IData* data) {
		return {
			static_cast<char const*>(data->data()),
			static_cast<size_t>(data->size()),
		};
	}

	std::filesystem::path repositoryRoot() {
		return std::filesystem::path(LUASTG_REPOSITORY_ROOT);
	}
}

TEST(FileSystemWindows, isFilePathCaseCorrect) {
	std::filesystem::path const path1(LR"(Hello world 你好世界.txt)"sv);
	std::ofstream file1(path1);
	file1.close();

	std::string c1;
	ASSERT_TRUE(win32::isFilePathCaseCorrect(R"(Hello world 你好世界.txt)"sv, c1));
	ASSERT_FALSE(win32::isFilePathCaseCorrect(R"(hello world 你好世界.txt)"sv, c1));
}

/*
TEST(FileSystemArchive, all) {
	core::SmartReference<core::IFileSystemArchive> archive;
	ASSERT_TRUE(core::IFileSystemArchive::createFromFile(R"(（窗口与显示分支）LuaSTG-Sub-v0.21.7.zip)"sv, archive.put()));

	ASSERT_FALSE(archive->hasFile("x"sv));
	ASSERT_FALSE(archive->hasDirectory("y"sv));

	ASSERT_TRUE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/"sv));
	ASSERT_FALSE(archive->hasFile("LuaSTG-Sub-v0.21.7/"sv));

	ASSERT_TRUE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/doc/"sv));
	ASSERT_TRUE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/license/"sv));
	ASSERT_TRUE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/res/"sv));
	ASSERT_TRUE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/src/"sv));
	ASSERT_TRUE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/windows-32bit/"sv));
	ASSERT_FALSE(archive->hasFile("LuaSTG-Sub-v0.21.7/doc/"sv));
	ASSERT_FALSE(archive->hasFile("LuaSTG-Sub-v0.21.7/license/"sv));
	ASSERT_FALSE(archive->hasFile("LuaSTG-Sub-v0.21.7/res/"sv));
	ASSERT_FALSE(archive->hasFile("LuaSTG-Sub-v0.21.7/src/"sv));
	ASSERT_FALSE(archive->hasFile("LuaSTG-Sub-v0.21.7/windows-32bit/"sv));

	ASSERT_TRUE(archive->hasFile("LuaSTG-Sub-v0.21.7/d3dcompiler_47.dll"sv));
	ASSERT_TRUE(archive->hasFile("LuaSTG-Sub-v0.21.7/LuaSTGSub.exe"sv));
	ASSERT_TRUE(archive->hasFile("LuaSTG-Sub-v0.21.7/xaudio2_9redist.dll"sv));
	ASSERT_FALSE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/d3dcompiler_47.dll"sv));
	ASSERT_FALSE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/LuaSTGSub.exe"sv));
	ASSERT_FALSE(archive->hasDirectory("LuaSTG-Sub-v0.21.7/xaudio2_9redist.dll"sv));
}
//*/

TEST(FileSystemOs, readFile) {
	if (!spdlog::get("test")) {
		spdlog::set_default_logger(spdlog::stdout_color_mt("test"));
	}

	auto const file_system = core::IFileSystemOS::getInstance();

	std::filesystem::path const p(u8"Windows.txt"sv);
	std::ofstream file(p, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
	file.close();

	core::SmartReference<core::IData> data;
	ASSERT_FALSE(file_system->readFile("windows.txt"sv, data.put()));
	ASSERT_TRUE(file_system->readFile("Windows.txt"sv, data.put()));
}

TEST(FileSystemOsEnumerator, all) {
	auto const file_system = core::IFileSystemOS::getInstance();

	core::SmartReference<core::IFileSystemEnumerator> enumerator;
	ASSERT_TRUE(file_system->createEnumerator(enumerator.put(), "Core.FileSystem.dir\\/"sv, true));

	while (enumerator->next()) {
		std::println("{}", enumerator->getName());
	}
}

TEST(FileSystemDATArchive, createAndLoadDat) {
	std::filesystem::path const root = std::filesystem::temp_directory_path() / "LuaSTG-Retro-DATArchive-Test";
	std::filesystem::path const source = root / "source";
	std::filesystem::path const archivePath = root / "archive.dat";

	std::filesystem::remove_all(root);
	writeBinaryFile(source / "ascii.txt", "hello dat archive");
	writeBinaryFile(source / "nested" / "large.bin", std::string(1024, 'x'));
	writeBinaryFile(source / u8"嵌套" / u8"素材.txt", "utf-8 path");

	core::DATArchiveCreator creator;
	creator.addFile("ascii.txt"sv);
	creator.addFile("nested/large.bin"sv);
	creator.addFile(reinterpret_cast<char const*>(u8"嵌套/素材.txt"));
	ASSERT_TRUE(creator.create(pathToUtf8(source), pathToUtf8(archivePath)));

	core::SmartReference<core::IFileSystemArchive> archive;
	ASSERT_TRUE(core::IFileSystemArchive::createFromFile(pathToUtf8(archivePath), archive.put()));
	ASSERT_TRUE(archive->hasFile("ascii.txt"sv));
	ASSERT_TRUE(archive->hasFile("nested/large.bin"sv));
	ASSERT_TRUE(archive->hasFile(reinterpret_cast<char const*>(u8"嵌套/素材.txt")));
	ASSERT_TRUE(archive->hasDirectory("nested"sv));
	ASSERT_TRUE(archive->hasDirectory(reinterpret_cast<char const*>(u8"嵌套")));
	ASSERT_FALSE(archive->hasFile("../ascii.txt"sv));

	core::SmartReference<core::IData> data;
	ASSERT_TRUE(archive->readFile("ascii.txt"sv, data.put()));
	ASSERT_EQ(readString(data.get()), "hello dat archive"sv);
	data = nullptr;

	ASSERT_TRUE(archive->readFile("nested/large.bin"sv, data.put()));
	ASSERT_EQ(readString(data.get()), std::string(1024, 'x'));
	data = nullptr;

	ASSERT_TRUE(archive->readFile(reinterpret_cast<char const*>(u8"嵌套/素材.txt"), data.put()));
	ASSERT_EQ(readString(data.get()), "utf-8 path"sv);

	std::filesystem::remove_all(root);
}

TEST(FileSystemDATArchive, rejectUnsafePathsOnCreate) {
	std::filesystem::path const root = std::filesystem::temp_directory_path() / "LuaSTG-Retro-DATArchive-UnsafePath-Test";
	std::filesystem::path const source = root / "source";
	std::filesystem::path const archivePath = root / "archive.dat";

	std::filesystem::remove_all(root);
	writeBinaryFile(source / "safe.txt", "safe");

	core::DATArchiveCreator creator;
	creator.addFile("../safe.txt"sv);
	ASSERT_FALSE(creator.create(pathToUtf8(source), pathToUtf8(archivePath)));

	core::DATArchiveCreator absolutePathCreator;
	absolutePathCreator.addFile("/safe.txt"sv);
	ASSERT_FALSE(absolutePathCreator.create(pathToUtf8(source), pathToUtf8(root / "absolute.dat")));

	std::filesystem::remove_all(root);
}

TEST(FileSystemDATArchive, crcMismatchDoesNotBlockRead) {
	std::filesystem::path const root = std::filesystem::temp_directory_path() / "LuaSTG-Retro-DATArchive-CRC-Test";
	std::filesystem::path const source = root / "source";
	std::filesystem::path const archivePath = root / "archive.dat";

	std::filesystem::remove_all(root);
	writeBinaryFile(source / "crc.txt", "crc-original");

	core::DATArchiveCreator creator;
	creator.addFile("crc.txt"sv);
	ASSERT_TRUE(creator.create(pathToUtf8(source), pathToUtf8(archivePath)));

	std::fstream archiveFile(archivePath, std::ios::binary | std::ios::in | std::ios::out);
	ASSERT_TRUE(archiveFile.is_open());
	archiveFile.seekg(static_cast<std::streamoff>(sizeof(core::DATArchiveHeader)), std::ios::beg);
	char byte{};
	archiveFile.read(&byte, sizeof(byte));
	ASSERT_TRUE(archiveFile.good());
	byte ^= 0x01;
	archiveFile.seekp(static_cast<std::streamoff>(sizeof(core::DATArchiveHeader)), std::ios::beg);
	archiveFile.write(&byte, sizeof(byte));
	ASSERT_TRUE(archiveFile.good());
	archiveFile.close();

	core::SmartReference<core::IFileSystemArchive> archive;
	ASSERT_TRUE(core::IFileSystemArchive::createFromFile(pathToUtf8(archivePath), archive.put()));

	core::SmartReference<core::IData> data;
	ASSERT_TRUE(archive->readFile("crc.txt"sv, data.put()));
	ASSERT_NE(readString(data.get()), "crc-original"sv);

	std::filesystem::remove_all(root);
}

TEST(FileSystemArchive, zipFallbackStillWorks) {
	std::filesystem::path const zipPath = repositoryRoot() / "data" / "test" / "assets" / "alpha.zip";
	ASSERT_TRUE(std::filesystem::exists(zipPath));

	core::SmartReference<core::IFileSystemArchive> archive;
	ASSERT_TRUE(core::IFileSystemArchive::createFromFile(pathToUtf8(zipPath), archive.put()));
	ASSERT_TRUE(archive->hasDirectory("alpha/"sv));
	ASSERT_TRUE(archive->hasFile("alpha/alpha.lua"sv));
}

/*
TEST(FileSystemOsEnumerator, recursive) {
	auto const file_system = core::IFileSystemOS::getInstance();

	core::SmartReference<core::IFileSystemEnumerator> enumerator;
	ASSERT_TRUE(file_system->createEnumerator(enumerator.put(), "Core.FileSystem.dir\\/"sv, true));

	while (enumerator->next()) {
		std::println("{}", enumerator->getName());
	}
}

TEST(FileSystemArchiveEnumerator, all) {
	core::SmartReference<core::IFileSystemArchive> archive;
	ASSERT_TRUE(core::IFileSystemArchive::createFromFile(R"(（窗口与显示分支）LuaSTG-Sub-v0.21.7.zip)"sv, archive.put()));

	core::SmartReference<core::IFileSystemEnumerator> enumerator;
	ASSERT_TRUE(archive->createEnumerator(enumerator.put(), "LuaSTG-Sub-v0.21.7/src"sv));

	while (enumerator->next()) {
		std::println("{}", enumerator->getName());
	}
}

TEST(FileSystemArchiveEnumerator, pattern) {
	core::SmartReference<core::IFileSystemArchive> archive;
	ASSERT_TRUE(core::IFileSystemArchive::createFromFile(R"(（窗口与显示分支）LuaSTG-Sub-v0.21.7.zip)"sv, archive.put()));

	core::SmartReference<core::IFileSystemEnumerator> enumerator;
	ASSERT_TRUE(archive->createEnumerator(enumerator.put(), "LuaSTG-Sub-v0.21.7////////////src"sv, true));

	while (enumerator->next()) {
		std::println("{}", enumerator->getName());
	}
}
//*/
