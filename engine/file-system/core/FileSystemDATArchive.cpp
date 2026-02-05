#include "core/FileSystemDATArchive.hpp"
#include "core/FileSystemCommon.hpp"   // isPathMatched, normalizePath, …
#include "core/SmartReference.hpp"
#include "core/Logger.hpp"
#include "utf8.hpp"

#if __has_include(<zlib-ng.h>)
#  include <zlib-ng.h>
#else
#  include <zlib.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>

// ─── file-local helpers ──────────────────────────────────────────────────────
namespace {

static void normalizeSlashes(std::string& path) {
	for (auto& c : path)
		if (c == '\\') c = '/';
}

// ── zlib wrappers ──────────────────────────────────────────────────────────

// Compress src → out (zlib-wrapper format).  Returns false on error.
static bool zlibDeflate(uint8_t const* src, size_t srcLen, std::vector<uint8_t>& out) {
	out.resize(compressBound(static_cast<uLong>(srcLen)));
	uLongf destLen = static_cast<uLongf>(out.size());
	int ret = compress2(out.data(), &destLen, src, static_cast<uLong>(srcLen), Z_DEFAULT_COMPRESSION);
	if (ret != Z_OK) { out.clear(); return false; }
	out.resize(destLen);
	return true;
}

// Decompress src → out using streaming inflate (output size need not be known in advance).
static bool zlibInflate(uint8_t const* src, size_t srcLen, std::vector<uint8_t>& out) {
	z_stream strm{};
	if (inflateInit(&strm) != Z_OK) return false;

	strm.next_in  = const_cast<Bytef*>(src);   // zlib does not take const*
	strm.avail_in = static_cast<uInt>(srcLen);

	uint8_t chunk[32768];
	int ret;
	do {
		strm.next_out  = chunk;
		strm.avail_out = static_cast<uInt>(sizeof(chunk));
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
			inflateEnd(&strm);
			return false;
		}
		size_t produced = sizeof(chunk) - strm.avail_out;
		out.insert(out.end(), chunk, chunk + produced);
	} while (ret != Z_STREAM_END);

	inflateEnd(&strm);
	return true;
}

// Decompress src → out when the expected uncompressed size IS known.
static bool zlibInflateKnown(uint8_t const* src, size_t srcLen,
                             std::vector<uint8_t>& out, size_t expectedSize) {
	out.resize(expectedSize);
	uLongf destLen = static_cast<uLongf>(expectedSize);
	int ret = uncompress(out.data(), &destLen, src, static_cast<uLong>(srcLen));
	if (ret != Z_OK) { out.clear(); return false; }
	out.resize(destLen);
	return true;
}

// ── entry-record serialization (binary layout matches v2) ─────────────────

// How many bytes the on-disk record occupies (excluding the leading uint32 size tag).
static size_t entryRecordSize(DATArchiveEntry const& entry) {
	std::wstring wpath = utf8::to_wstring(entry.path);
	return   wpath.size() * sizeof(wchar_t) + sizeof(uint32_t)   // pathCharCount + chars
	       + sizeof(uint8_t)                                      // compressionType
	       + sizeof(uint32_t) * 4                                 // sizeFull, sizeStored, offsetPos, crc32
	       + sizeof(uint8_t) * 2;                                 // keyBase, keyStep
}

// Append the serialised record for one entry into buf.
static void writeEntryRecord(std::vector<uint8_t>& buf, DATArchiveEntry const& entry) {
	auto append = [&](void const* data, size_t len) {
		buf.insert(buf.end(),
		           static_cast<uint8_t const*>(data),
		           static_cast<uint8_t const*>(data) + len);
	};

	std::wstring wpath = utf8::to_wstring(entry.path);
	uint32_t charCount = static_cast<uint32_t>(wpath.size());
	append(&charCount,        sizeof(charCount));
	append(wpath.data(),      charCount * sizeof(wchar_t));

	uint8_t ct = static_cast<uint8_t>(entry.compressionType);
	append(&ct,               sizeof(ct));
	append(&entry.sizeFull,   sizeof(entry.sizeFull));
	append(&entry.sizeStored, sizeof(entry.sizeStored));
	append(&entry.offsetPos,  sizeof(entry.offsetPos));
	append(&entry.keyBase,    sizeof(entry.keyBase));
	append(&entry.keyStep,    sizeof(entry.keyStep));
	append(&entry.crc32Value, sizeof(entry.crc32Value));
}

// Parse one entry record from a byte buffer.  `cursor` is advanced past the record.
static bool readEntryRecord(uint8_t const*& cursor, uint8_t const* end,
                            DATArchiveEntry& entry, bool hasCrc) {
	auto read = [&](void* dest, size_t len) -> bool {
		if (cursor + static_cast<ptrdiff_t>(len) > end) return false;
		std::memcpy(dest, cursor, len);
		cursor += len;
		return true;
	};

	uint32_t charCount = 0;
	if (!read(&charCount, sizeof(charCount))) return false;
	if (static_cast<size_t>(charCount) * sizeof(wchar_t) > static_cast<size_t>(end - cursor)) return false;

	std::wstring wpath(charCount, L'\0');
	if (!read(wpath.data(), charCount * sizeof(wchar_t))) return false;

	entry.path = utf8::to_string(wpath);
	normalizeSlashes(entry.path);

	uint8_t ct = 0;
	if (!read(&ct, sizeof(ct))) return false;
	entry.compressionType = static_cast<DATArchiveEntry::CompressionType>(ct);

	if (!read(&entry.sizeFull,   sizeof(entry.sizeFull)))   return false;
	if (!read(&entry.sizeStored, sizeof(entry.sizeStored))) return false;
	if (!read(&entry.offsetPos,  sizeof(entry.offsetPos)))  return false;
	if (!read(&entry.keyBase,    sizeof(entry.keyBase)))    return false;
	if (!read(&entry.keyStep,    sizeof(entry.keyStep)))    return false;

	entry.crc32Value = 0;
	if (hasCrc) {
		if (!read(&entry.crc32Value, sizeof(entry.crc32Value))) return false;
	}
	return true;
}

// ── directory helpers ──────────────────────────────────────────────────────

static void collectParentDirectories(std::string_view const& filePath,
                                     std::set<std::string, std::less<>>& dirs) {
	size_t pos = 0;
	while ((pos = filePath.find('/', pos)) != std::string_view::npos) {
		dirs.emplace(filePath.substr(0, pos + 1));
		++pos;
	}
}

} // anonymous namespace

// ─── FileSystemDATArchive ────────────────────────────────────────────────────

FileSystemDATArchive::~FileSystemDATArchive() {
	if (m_file.is_open()) m_file.close();
}

bool FileSystemDATArchive::open(std::string_view const& path, size_t readOffset) {
	std::lock_guard lock(m_mutex);

	m_path       = path;
	m_readOffset = readOffset;
	m_entries.clear();
	m_directories.clear();

	std::string pathStr(path);
	m_file.open(pathStr, std::ios::binary);
	if (!m_file.is_open()) {
		Logger::error("FileSystemDATArchive: cannot open '{}'", path);
		return false;
	}

	// ── read raw header bytes ─────────────────────────────────────────────
	constexpr std::string_view masterKey{ ArchiveEncryption::ENCRYPTION_KEY,
	                                      ArchiveEncryption::ENCRYPTION_KEY_LEN };
	alignas(DATArchiveHeader) uint8_t headerBuf[sizeof(DATArchiveHeader)];

	m_file.seekg(static_cast<std::streamoff>(m_readOffset), std::ios::beg);
	m_file.read(reinterpret_cast<char*>(headerBuf), sizeof(headerBuf));
	if (!m_file) {
		Logger::error("FileSystemDATArchive: failed to read header from '{}'", path);
		return false;
	}

	// ── try v2 decryption ─────────────────────────────────────────────────
	uint8_t keyBase, keyStep;
	DATArchiveHeader header{};
	bool isLegacy = false;

	ArchiveEncryption::getKeyHashHeader(masterKey, keyBase, keyStep);
	{
		uint8_t base = keyBase;
		uint8_t buf[sizeof(DATArchiveHeader)];
		std::memcpy(buf, headerBuf, sizeof(buf));
		ArchiveEncryption::shiftBlock(buf, sizeof(buf), base, keyStep);
		std::memcpy(&header, buf, sizeof(header));

		if (std::memcmp(header.magic, ArchiveEncryption::HEADER_MAGIC, 8) == 0) {
			m_keyBase = base;   // advanced past header
			m_keyStep = keyStep;
		}
		else {
			// ── fall back to v1 (legacy) ────────────────────────────────
			ArchiveEncryption::getKeyHashHeaderLegacy(masterKey, keyBase, keyStep);
			base = keyBase;
			uint8_t buf2[sizeof(DATArchiveHeader)];
			std::memcpy(buf2, headerBuf, sizeof(buf2));
			ArchiveEncryption::shiftBlockLegacy(buf2, sizeof(buf2), base, keyStep);
			std::memcpy(&header, buf2, sizeof(header));

			if (std::memcmp(header.magic, ArchiveEncryption::HEADER_MAGIC, 8) != 0) {
				Logger::error("FileSystemDATArchive: '{}' is not a valid DAT archive", path);
				return false;
			}
			isLegacy   = true;
			m_keyBase  = base;
			m_keyStep  = keyStep;
		}
	}

	m_version = header.version;
	if (m_version != ArchiveEncryption::VERSION_CURRENT &&
	    m_version != ArchiveEncryption::VERSION_LEGACY) {
		Logger::error("FileSystemDATArchive: unsupported version {} in '{}'", m_version, path);
		return false;
	}

	// ── read & decrypt metadata block ─────────────────────────────────────
	m_file.clear();
	m_file.seekg(static_cast<std::streamoff>(m_readOffset + header.headerOffset), std::ios::beg);

	std::vector<uint8_t> encMeta(header.headerSize);
	m_file.read(reinterpret_cast<char*>(encMeta.data()), header.headerSize);
	if (!m_file) {
		Logger::error("FileSystemDATArchive: failed to read metadata from '{}'", path);
		return false;
	}

	// Decrypt continues the header keystream (m_keyBase was already advanced past header).
	if (isLegacy)
		ArchiveEncryption::shiftBlockLegacy(encMeta.data(), encMeta.size(), m_keyBase, m_keyStep);
	else
		ArchiveEncryption::shiftBlock(encMeta.data(), encMeta.size(), m_keyBase, m_keyStep);

	// ── decompress metadata ───────────────────────────────────────────────
	std::vector<uint8_t> metaBuf;
	if (!zlibInflate(encMeta.data(), encMeta.size(), metaBuf)) {
		Logger::error("FileSystemDATArchive: failed to decompress metadata in '{}'", path);
		return false;
	}

	// ── parse entry records ───────────────────────────────────────────────
	bool        hasCrc = (m_version != ArchiveEncryption::VERSION_LEGACY);
	uint8_t const* cursor = metaBuf.data();
	uint8_t const* end    = metaBuf.data() + metaBuf.size();

	for (uint32_t i = 0; i < header.entryCount; ++i) {
		// Each record is preceded by a uint32 size tag (consumed but not used for skipping).
		uint32_t recordSize = 0;
		if (cursor + static_cast<ptrdiff_t>(sizeof(uint32_t)) > end) {
			Logger::warn("FileSystemDATArchive: truncated metadata at entry {}", i);
			break;
		}
		std::memcpy(&recordSize, cursor, sizeof(uint32_t));
		cursor += sizeof(uint32_t);

		DATArchiveEntry entry{};
		if (!readEntryRecord(cursor, end, entry, hasCrc)) {
			Logger::warn("FileSystemDATArchive: failed to parse entry {} in '{}'", i, path);
			break;
		}

		collectParentDirectories(entry.path, m_directories);
		m_entries.emplace(entry.path, std::move(entry));
	}

	return true;
}

std::string_view FileSystemDATArchive::getArchivePath() { return m_path; }

bool FileSystemDATArchive::setPassword(std::string_view const& /*password*/) {
	return false;  // DAT archives use a built-in key; password is not applicable.
}

// ── IFileSystem queries ───────────────────────────────────────────────────

bool FileSystemDATArchive::hasNode(std::string_view const& name) {
	std::lock_guard lock(m_mutex);
	if (m_entries.count(name)) return true;
	// Normalise for directory check: ensure trailing slash.
	std::string dir(name);
	if (!dir.empty() && dir.back() != '/') dir += '/';
	return m_directories.count(dir) > 0;
}

FileSystemNodeType FileSystemDATArchive::getNodeType(std::string_view const& name) {
	std::lock_guard lock(m_mutex);
	if (m_entries.count(name))  return FileSystemNodeType::file;
	std::string dir(name);
	if (!dir.empty() && dir.back() != '/') dir += '/';
	if (m_directories.count(dir)) return FileSystemNodeType::directory;
	return FileSystemNodeType::unknown;
}

bool FileSystemDATArchive::hasFile(std::string_view const& name) {
	std::lock_guard lock(m_mutex);
	return m_entries.count(name) > 0;
}

size_t FileSystemDATArchive::getFileSize(std::string_view const& name) {
	std::lock_guard lock(m_mutex);
	auto it = m_entries.find(name);
	return it != m_entries.end() ? it->second.sizeFull : 0;
}

bool FileSystemDATArchive::hasDirectory(std::string_view const& name) {
	std::lock_guard lock(m_mutex);
	if (name.empty()) return true;  // root always exists
	std::string dir(name);
	if (!dir.empty() && dir.back() != '/') dir += '/';
	return m_directories.count(dir) > 0;
}

// ── file reading ──────────────────────────────────────────────────────────

bool FileSystemDATArchive::readFile(std::string_view const& name, IData** const data) {
	std::lock_guard lock(m_mutex);
	if (!data) return false;
	auto it = m_entries.find(name);
	if (it == m_entries.end()) return false;
	return readEntryData(it->second, data);
}

bool FileSystemDATArchive::readEntryData(DATArchiveEntry const& entry, IData** const data) {
	if (!m_file.is_open()) return false;

	m_file.clear();
	m_file.seekg(static_cast<std::streamoff>(m_readOffset + entry.offsetPos), std::ios::beg);

	std::vector<uint8_t> rawBuf(entry.sizeStored);
	if (entry.sizeStored > 0) {
		m_file.read(reinterpret_cast<char*>(rawBuf.data()), entry.sizeStored);
		if (!m_file) return false;
	}

	// Decrypt with per-file key.
	uint8_t base = entry.keyBase;
	if (m_version == ArchiveEncryption::VERSION_LEGACY)
		ArchiveEncryption::shiftBlockLegacy(rawBuf.data(), rawBuf.size(), base, entry.keyStep);
	else
		ArchiveEncryption::shiftBlock(rawBuf.data(), rawBuf.size(), base, entry.keyStep);

	// Decompress if needed and produce the final buffer.
	std::vector<uint8_t> outBuf;
	switch (entry.compressionType) {
	case DATArchiveEntry::CT_NONE:
		outBuf = std::move(rawBuf);
		break;

	case DATArchiveEntry::CT_ZLIB:
	{
		if (entry.sizeStored > 0) {
			if (!zlibInflateKnown(rawBuf.data(), rawBuf.size(), outBuf, entry.sizeFull)) {
				Logger::warn("FileSystemDATArchive: inflate failed for '{}'", entry.path);
				return false;
			}
		}
		if (outBuf.size() != entry.sizeFull) {
			Logger::warn("FileSystemDATArchive: size mismatch after inflate for '{}' "
			             "(expected {} got {})",
			             entry.path, entry.sizeFull, outBuf.size());
		}
		break;
	}
	}

	// CRC-32 verification.
	if (entry.crc32Value != 0 && !outBuf.empty()) {
		uint32_t actual = static_cast<uint32_t>(
			crc32(0, outBuf.data(), static_cast<uInt>(outBuf.size())));
		if (actual != entry.crc32Value) {
			Logger::warn("FileSystemDATArchive: CRC mismatch for '{}' "
			             "(expected 0x{:08X}, got 0x{:08X})",
			             entry.path, entry.crc32Value, actual);
		}
	}

	// Wrap in IData.
	SmartReference<IData> result;
	if (!IData::create(outBuf.size(), result.put())) return false;
	if (!outBuf.empty())
		std::memcpy(result->data(), outBuf.data(), outBuf.size());

	*data = result.detach();
	return true;
}

// ── enumerator creation ─────────────────────────────────────────────────

bool FileSystemDATArchive::createEnumerator(IFileSystemEnumerator** const enumerator,
                                            std::string_view const& directory, bool const recursive) {
	m_mutex.lock();  // released by the enumerator's destructor
	*enumerator = new FileSystemDATArchiveEnumerator(this, directory, recursive);
	return true;
}

// ── factory ─────────────────────────────────────────────────────────────

bool FileSystemDATArchive::createFromFile(std::string_view const& path,
                                          IFileSystemArchive** const archive) {
	return createFromFile(path, 0, archive);
}

bool FileSystemDATArchive::createFromFile(std::string_view const& path, size_t readOffset,
                                          IFileSystemArchive** const archive) {
	SmartReference<FileSystemDATArchive> obj;
	obj.attach(new FileSystemDATArchive());
	if (!obj->open(path, readOffset)) return false;
	*archive = obj.detach();
	return true;
}

// ─── FileSystemDATArchiveEnumerator ──────────────────────────────────────────

FileSystemDATArchiveEnumerator::FileSystemDATArchiveEnumerator(
		FileSystemDATArchive* const archive,
		std::string_view const&    directory,
		bool const                 recursive)
	: m_archive(archive)
{
	assert(archive != nullptr);

	// Normalise the directory the same way FileSystemArchiveEnumerator does.
	std::string dir;
	if (!directory.empty()) {
		std::u8string const normalized = normalizePath(directory, true);
		if (!normalized.empty()) {
			std::string_view nv = getStringView(normalized);
			if (!isPathEndsWithSeparator(nv)) {
				dir.reserve(nv.size() + 1);
				dir.append(nv);
				dir.push_back('/');
			}
			else {
				dir.assign(nv);
			}
		}
	}

	// Collect matching files.
	for (auto const& [path, entry] : archive->m_entries) {
		if (isPathMatched(path, dir, recursive)) {
			m_items.push_back({ path, false, entry.sizeFull });
		}
	}
	// Collect matching synthetic directories.
	for (auto const& dirPath : archive->m_directories) {
		if (isPathMatched(dirPath, dir, recursive)) {
			m_items.push_back({ dirPath, true, 0 });
		}
	}
}

FileSystemDATArchiveEnumerator::~FileSystemDATArchiveEnumerator() {
	m_archive->m_mutex.unlock();
}

bool FileSystemDATArchiveEnumerator::next() {
	++m_index;
	return m_index >= 0 && static_cast<size_t>(m_index) < m_items.size();
}

std::string_view FileSystemDATArchiveEnumerator::getName() {
	if (m_index < 0 || static_cast<size_t>(m_index) >= m_items.size()) return {};
	return m_items[m_index].name;
}

FileSystemNodeType FileSystemDATArchiveEnumerator::getNodeType() {
	if (m_index < 0 || static_cast<size_t>(m_index) >= m_items.size())
		return FileSystemNodeType::unknown;
	return m_items[m_index].isDirectory ? FileSystemNodeType::directory
	                                    : FileSystemNodeType::file;
}

size_t FileSystemDATArchiveEnumerator::getFileSize() {
	if (m_index < 0 || static_cast<size_t>(m_index) >= m_items.size()) return 0;
	return m_items[m_index].fileSize;
}

bool FileSystemDATArchiveEnumerator::readFile(IData** const data) {
	if (m_index < 0 || static_cast<size_t>(m_index) >= m_items.size()) return false;
	if (m_items[m_index].isDirectory) return false;

	auto it = m_archive->m_entries.find(m_items[m_index].name);
	if (it == m_archive->m_entries.end()) return false;
	// Lock is already held for the lifetime of this enumerator.
	return m_archive->readEntryData(it->second, data);
}

// ─── DATArchiveCreator ───────────────────────────────────────────────────────

void DATArchiveCreator::addFile(std::string_view const& relativePath) {
	std::string p(relativePath);
	normalizeSlashes(p);
	m_files.push_back(std::move(p));
}

bool DATArchiveCreator::create(std::string_view const& baseDir,
                               std::string_view const& outputPath,
                               StatusCallback   onStatus,
                               ProgressCallback onProgress) {
	auto status   = [&](std::string_view msg) { if (onStatus)   onStatus(msg);   };
	auto progress = [&](float v)              { if (onProgress) onProgress(v);   };

	progress(0.0f);

	// ── derive header-level key ─────────────────────────────────────────
	uint8_t headerKeyBase, headerKeyStep;
	ArchiveEncryption::getKeyHashHeader(
		std::string_view(ArchiveEncryption::ENCRYPTION_KEY, ArchiveEncryption::ENCRYPTION_KEY_LEN),
		headerKeyBase, headerKeyStep);

	// Normalise base directory.
	std::string base(baseDir);
	normalizeSlashes(base);
	if (!base.empty() && base.back() != '/') base.push_back('/');

	// ── open temp file ──────────────────────────────────────────────────
	std::string tmpPath = std::string(outputPath) + ".tmp";
	std::fstream tmpFile(tmpPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
	if (!tmpFile.is_open()) {
		Logger::error("DATArchiveCreator: cannot create temp file '{}'", tmpPath);
		return false;
	}

	status("Writing header");

	// ── write header stub ───────────────────────────────────────────────
	DATArchiveHeader header{};
	std::memcpy(header.magic, ArchiveEncryption::HEADER_MAGIC, sizeof(header.magic));
	header.version    = ArchiveEncryption::VERSION_CURRENT;
	header.entryCount = static_cast<uint32_t>(m_files.size());
	// headerOffset / headerSize filled later.
	tmpFile.write(reinterpret_cast<char const*>(&header), sizeof(header));

	progress(0.1f);

	// ── write file data ─────────────────────────────────────────────────
	std::vector<DATArchiveEntry> entries;
	entries.reserve(m_files.size());
	float progressStep = m_files.empty() ? 0.0f : (0.75f - 0.10f) / static_cast<float>(m_files.size());

	for (size_t i = 0; i < m_files.size(); ++i) {
		status(std::format("Processing [{}]", m_files[i]));

		std::string fullPath = base + m_files[i];
		std::ifstream file(fullPath, std::ios::binary);
		if (!file.is_open()) {
			Logger::error("DATArchiveCreator: cannot open '{}'", fullPath);
			std::filesystem::remove(tmpPath);
			return false;
		}

		file.seekg(0, std::ios::end);
		uint32_t fileSize = static_cast<uint32_t>(file.tellg());
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> content(fileSize);
		if (fileSize > 0) {
			file.read(reinterpret_cast<char*>(content.data()), fileSize);
		}
		file.close();

		DATArchiveEntry entry;
		entry.path        = m_files[i];
		entry.sizeFull    = fileSize;
		entry.sizeStored  = fileSize;
		entry.offsetPos   = static_cast<uint32_t>(tmpFile.tellp());

		// CRC-32 on the original (uncompressed) data.
		entry.crc32Value  = static_cast<uint32_t>(crc32(0, content.data(), static_cast<uInt>(fileSize)));

		// Per-file encryption key (derived from the relative path).
		ArchiveEncryption::getKeyHashFile(m_files[i], headerKeyBase, headerKeyStep,
		                                  entry.keyBase, entry.keyStep);

		// Compress files >= 256 bytes; smaller files grow under zlib overhead.
		if (fileSize >= 0x100) {
			entry.compressionType = DATArchiveEntry::CT_ZLIB;
			std::vector<uint8_t> compressed;
			if (zlibDeflate(content.data(), fileSize, compressed)) {
				entry.sizeStored       = static_cast<uint32_t>(compressed.size());
				content                = std::move(compressed);
			}
			else {
				// Fall back to uncompressed on failure.
				entry.compressionType = DATArchiveEntry::CT_NONE;
				entry.sizeStored      = fileSize;
			}
		}

		// Write the (possibly compressed) data — still unencrypted at this point.
		if (!content.empty())
			tmpFile.write(reinterpret_cast<char const*>(content.data()), entry.sizeStored);

		entries.push_back(std::move(entry));
		progress(0.1f + progressStep * static_cast<float>(i));
	}

	// ── write metadata ──────────────────────────────────────────────────
	status("Writing entries info");
	std::streampos metaBegin = tmpFile.tellp();
	tmpFile.flush();

	// Serialise all entry records into a flat buffer.
	std::vector<uint8_t> metaBuf;
	for (auto const& entry : entries) {
		uint32_t recSize = static_cast<uint32_t>(entryRecordSize(entry));
		metaBuf.insert(metaBuf.end(),
		               reinterpret_cast<uint8_t const*>(&recSize),
		               reinterpret_cast<uint8_t const*>(&recSize) + sizeof(recSize));
		writeEntryRecord(metaBuf, entry);
	}

	// Compress metadata.
	std::vector<uint8_t> compMeta;
	if (!zlibDeflate(metaBuf.data(), metaBuf.size(), compMeta)) {
		Logger::error("DATArchiveCreator: failed to compress metadata");
		std::filesystem::remove(tmpPath);
		return false;
	}

	tmpFile.write(reinterpret_cast<char const*>(compMeta.data()), compMeta.size());

	// Patch header with final metadata offset / size.
	header.headerOffset = static_cast<uint32_t>(metaBegin);
	header.headerSize   = static_cast<uint32_t>(compMeta.size());

	tmpFile.seekp(static_cast<std::streamoff>(offsetof(DATArchiveHeader, headerOffset)));
	tmpFile.write(reinterpret_cast<char const*>(&header.headerOffset), sizeof(uint32_t));
	tmpFile.write(reinterpret_cast<char const*>(&header.headerSize),   sizeof(uint32_t));
	tmpFile.flush();

	// ── encrypt temp → output ──────────────────────────────────────────
	status("Encrypting archive");
	progress(0.95f);

	bool ok = encryptArchive(tmpFile, outputPath, header, headerKeyBase, headerKeyStep, entries);
	tmpFile.close();
	std::filesystem::remove(tmpPath);

	if (!ok) {
		Logger::error("DATArchiveCreator: encryption pass failed");
		return false;
	}

	status("Done");
	progress(1.0f);
	return true;
}

bool DATArchiveCreator::encryptArchive(std::fstream&                src,
                                       std::string_view const&      outputPath,
                                       DATArchiveHeader const&      header,
                                       uint8_t                      keyBase,
                                       uint8_t                      keyStep,
                                       std::vector<DATArchiveEntry> const& entries) {
	if (!src.is_open()) return false;
	src.clear();
	src.seekg(0);

	std::ofstream dest(std::string(outputPath), std::ios::binary | std::ios::trunc);
	if (!dest.is_open()) return false;

	constexpr size_t CHUNK = 16384;
	std::vector<uint8_t> buf(CHUNK);

	uint8_t headerBase = keyBase;   // will be advanced past the 24-byte header

	// 1) Encrypt & copy the header.
	src.read(reinterpret_cast<char*>(buf.data()), sizeof(DATArchiveHeader));
	ArchiveEncryption::shiftBlock(buf.data(), sizeof(DATArchiveHeader), headerBase, keyStep);
	dest.write(reinterpret_cast<char const*>(buf.data()), sizeof(DATArchiveHeader));

	// 2) Encrypt each file's data block with its own independent key stream.
	for (auto const& entry : entries) {
		size_t  remaining = entry.sizeStored;
		uint8_t localBase = entry.keyBase;

		src.clear();
		src.seekg(static_cast<std::streamoff>(entry.offsetPos), std::ios::beg);
		dest.seekp(static_cast<std::streamoff>(entry.offsetPos), std::ios::beg);

		while (remaining > 0) {
			size_t toRead = std::min(remaining, CHUNK);
			src.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(toRead));
			size_t got = static_cast<size_t>(src.gcount());
			if (got > remaining) got = remaining;

			ArchiveEncryption::shiftBlock(buf.data(), got, localBase, entry.keyStep);
			dest.write(reinterpret_cast<char const*>(buf.data()), static_cast<std::streamsize>(got));

			remaining -= got;
			if (got == 0) break;
		}
	}

	// 3) Encrypt the metadata block, continuing the header keystream.
	//    headerBase was already advanced by step 1.
	{
		size_t remaining = header.headerSize;

		src.clear();
		src.seekg(static_cast<std::streamoff>(header.headerOffset), std::ios::beg);
		dest.seekp(static_cast<std::streamoff>(header.headerOffset), std::ios::beg);

		while (remaining > 0) {
			size_t toRead = std::min(remaining, CHUNK);
			src.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(toRead));
			size_t got = static_cast<size_t>(src.gcount());
			if (got > remaining) got = remaining;

			ArchiveEncryption::shiftBlock(buf.data(), got, headerBase, keyStep);
			dest.write(reinterpret_cast<char const*>(buf.data()), static_cast<std::streamsize>(got));

			remaining -= got;
			if (got == 0) break;
		}
	}

	dest.close();
	return true;
}
