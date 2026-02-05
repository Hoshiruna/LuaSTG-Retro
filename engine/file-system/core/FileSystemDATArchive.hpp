#pragma once
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include "core/ArchiveEncryption.hpp"

#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace core {

// ─── on-disk header (24 bytes, packed, little-endian) ────────────────────────
#pragma pack(push, 1)
struct DATArchiveHeader {
	char     magic[8];        // first 8 bytes of ArchiveEncryption::HEADER_MAGIC
	uint32_t version;         // 1 (legacy) or 2 (current)
	uint32_t entryCount;      // number of file entries in the archive
	uint32_t headerOffset;    // byte-offset of the compressed metadata block
	uint32_t headerSize;      // byte-length of the compressed metadata block
};
#pragma pack(pop)

// ─── in-memory entry descriptor ──────────────────────────────────────────────
struct DATArchiveEntry {
	enum CompressionType : uint8_t { CT_NONE = 0, CT_ZLIB = 1 };

	std::string     path;                       // UTF-8 relative path
	CompressionType compressionType{ CT_NONE };
	uint32_t        sizeFull{};                 // uncompressed size
	uint32_t        sizeStored{};               // on-disk size (after compression)
	uint32_t        offsetPos{};                // byte-offset inside the archive
	uint8_t         keyBase{};                  // per-file cipher seed
	uint8_t         keyStep{};                  // per-file cipher step
	uint32_t        crc32Value{};               // CRC-32 of the uncompressed data
};

// ─── reader (IFileSystemArchive) ─────────────────────────────────────────────
// Reads LSTGRETROARC (.dat) archives.  Supports both v1 (legacy) and v2 formats.
class FileSystemDATArchive final : public implement::ReferenceCounted<IFileSystemArchive> {
	friend class FileSystemDATArchiveEnumerator;
public:
	// IFileSystem
	bool               hasNode(std::string_view const& name) override;
	FileSystemNodeType getNodeType(std::string_view const& name) override;
	bool               hasFile(std::string_view const& name) override;
	size_t             getFileSize(std::string_view const& name) override;
	bool               readFile(std::string_view const& name, IData** data) override;
	bool               hasDirectory(std::string_view const& name) override;
	bool               createEnumerator(IFileSystemEnumerator** enumerator,
	                                    std::string_view const& directory, bool recursive) override;

	// IFileSystemArchive
	std::string_view getArchivePath() override;
	bool             setPassword(std::string_view const& password) override;

	// Lifecycle
	FileSystemDATArchive()                                      = default;
	FileSystemDATArchive(FileSystemDATArchive const&)           = delete;
	FileSystemDATArchive(FileSystemDATArchive&&)                = delete;
	~FileSystemDATArchive() override;
	FileSystemDATArchive& operator=(FileSystemDATArchive const&) = delete;
	FileSystemDATArchive& operator=(FileSystemDATArchive&&)      = delete;

	// Open an LSTGRETROARC archive.  readOffset allows the archive to be
	// embedded inside a larger container file (e.g. an executable stub).
	bool open(std::string_view const& path, size_t readOffset = 0);

	// Factory helpers – follow the same pattern as IFileSystemArchive::createFromFile.
	static bool createFromFile(std::string_view const& path,
	                           IFileSystemArchive** archive);
	static bool createFromFile(std::string_view const& path, size_t readOffset,
	                           IFileSystemArchive** archive);

private:
	bool readEntryData(DATArchiveEntry const& entry, IData** data);

	std::string          m_path;
	std::fstream         m_file;
	size_t               m_readOffset{};
	uint8_t              m_keyBase{};           // header keystream – advanced past the header on open()
	uint8_t              m_keyStep{};
	uint32_t             m_version{};
	std::recursive_mutex m_mutex;

	std::map<std::string, DATArchiveEntry, std::less<>> m_entries;      // transparent lookup
	std::set<std::string, std::less<>>                  m_directories;  // synthetic, trailing '/'
};

// ─── enumerator ──────────────────────────────────────────────────────────────
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

// ─── writer ──────────────────────────────────────────────────────────────────
// Creates an LSTGRETROARC v2 archive from a directory of files.
class DATArchiveCreator {
public:
	using StatusCallback   = std::function<void(std::string_view const&)>;
	using ProgressCallback = std::function<void(float)>;

	// Schedule a file for inclusion.  `relativePath` is relative to the
	// baseDir passed to create().
	void addFile(std::string_view const& relativePath);

	// Write the encrypted archive.  Paths use forward slashes internally;
	// the OS separator is accepted for baseDir / outputPath on input.
	bool create(std::string_view const& baseDir,
	            std::string_view const& outputPath,
	            StatusCallback   onStatus   = {},
	            ProgressCallback onProgress = {});

private:
	std::vector<std::string> m_files;

	static bool encryptArchive(std::fstream& src,
	                           std::string_view const& outputPath,
	                           DATArchiveHeader const& header,
	                           uint8_t keyBase, uint8_t keyStep,
	                           std::vector<DATArchiveEntry> const& entries);
};

} // namespace core
