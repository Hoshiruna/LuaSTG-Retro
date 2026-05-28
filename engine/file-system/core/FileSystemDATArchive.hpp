#pragma once
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include "core/DATArchiveEncryption.hpp"

#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace core {

#pragma pack(push, 1)
struct DATArchiveHeader {
	char     magic[8];
	uint32_t entryCount;
	uint32_t headerOffset;
	uint32_t headerSize;
};
#pragma pack(pop)

struct DATArchiveEntry {
	enum CompressionType : uint8_t { CT_NONE = 0, CT_ZLIB = 1 };

	std::string     path;
	CompressionType compressionType{ CT_NONE };
	uint32_t        sizeFull{};
	uint32_t        sizeStored{};
	uint32_t        offsetPos{};
	uint8_t         keyBase{};
	uint8_t         keyStep{};
	uint32_t        crc32Value{};
};

class FileSystemDATArchive final : public implement::ReferenceCounted<IFileSystemArchive> {
	friend class FileSystemDATArchiveEnumerator;
public:
	bool               hasNode(std::string_view const& name) override;
	FileSystemNodeType getNodeType(std::string_view const& name) override;
	bool               hasFile(std::string_view const& name) override;
	size_t             getFileSize(std::string_view const& name) override;
	bool               readFile(std::string_view const& name, IData** data) override;
	bool               hasDirectory(std::string_view const& name) override;
	bool               createEnumerator(IFileSystemEnumerator** enumerator,
	                                    std::string_view const& directory, bool recursive) override;

	std::string_view getArchivePath() override;
	bool             setPassword(std::string_view const& password) override;

	FileSystemDATArchive()                                      = default;
	FileSystemDATArchive(FileSystemDATArchive const&)           = delete;
	FileSystemDATArchive(FileSystemDATArchive&&)                = delete;
	~FileSystemDATArchive() override;
	FileSystemDATArchive& operator=(FileSystemDATArchive const&) = delete;
	FileSystemDATArchive& operator=(FileSystemDATArchive&&)      = delete;

	bool open(std::string_view const& path, size_t readOffset = 0);

	static bool createFromFile(std::string_view const& path,
	                           IFileSystemArchive** archive);
	static bool createFromFile(std::string_view const& path, size_t readOffset,
	                           IFileSystemArchive** archive);

private:
	bool readEntryData(DATArchiveEntry const& entry, IData** data);

	std::string          m_path;
	std::fstream         m_file;
	size_t               m_readOffset{};
	uint8_t              m_keyBase{};
	uint8_t              m_keyStep{};
	std::recursive_mutex m_mutex;

	std::map<std::string, DATArchiveEntry, std::less<>> m_entries;
	std::set<std::string, std::less<>>                  m_directories;
};

class FileSystemDATArchiveEnumerator final : public implement::ReferenceCounted<IFileSystemEnumerator> {
public:
	bool               next() override;
	std::string_view   getName() override;
	FileSystemNodeType getNodeType() override;
	size_t             getFileSize() override;
	bool               readFile(IData** data) override;

	FileSystemDATArchiveEnumerator(FileSystemDATArchive* archive,
	                               std::string_view const& directory, bool recursive);
	~FileSystemDATArchiveEnumerator() override;

	FileSystemDATArchiveEnumerator(FileSystemDATArchiveEnumerator const&)           = delete;
	FileSystemDATArchiveEnumerator(FileSystemDATArchiveEnumerator&&)                = delete;
	FileSystemDATArchiveEnumerator& operator=(FileSystemDATArchiveEnumerator const&) = delete;
	FileSystemDATArchiveEnumerator& operator=(FileSystemDATArchiveEnumerator&&)      = delete;

private:
	struct Item {
		std::string name;
		bool        isDirectory{};
		uint32_t    fileSize{};
	};

	SmartReference<FileSystemDATArchive> m_archive;
	std::vector<Item>                    m_items;
	int                                  m_index{ -1 };
};

class DATArchiveCreator {
public:
	void addFile(std::string_view const& relativePath);

	bool create(std::string_view const& baseDir,
	            std::string_view const& outputPath);

private:
	std::vector<std::string> m_files;

	static bool encryptArchive(std::fstream& src,
	                           std::string_view const& outputPath,
	                           DATArchiveHeader const& header,
	                           uint8_t keyBase, uint8_t keyStep,
	                           std::vector<DATArchiveEntry> const& entries);
};

} // namespace core
