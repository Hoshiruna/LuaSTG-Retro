#include "core/FileSystemDATArchive.hpp"
#include "core/FileSystemCommon.hpp"
#include "core/SmartReference.hpp"
#include "core/Logger.hpp"
#include "utf8.hpp"

#include <zlib-ng.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <limits>

namespace core {

namespace {

static void normalizeSlashes(std::string& path) {
	for (auto& c : path)
		if (c == '\\') c = '/';
}

static bool zlibDeflate(uint8_t const* src, size_t srcLen, std::vector<uint8_t>& out) {
	out.resize(zng_compressBound(srcLen));
	size_t destLen = out.size();
	int ret = zng_compress2(out.data(), &destLen, src, srcLen, Z_DEFAULT_COMPRESSION);
	if (ret != Z_OK) { out.clear(); return false; }
	out.resize(destLen);
	return true;
}

static bool zlibInflate(uint8_t const* src, size_t srcLen, std::vector<uint8_t>& out) {
	zng_stream strm{};
	if (zng_inflateInit(&strm) != Z_OK) return false;

	strm.next_in  = const_cast<uint8_t*>(src);
	strm.avail_in = static_cast<uint32_t>(srcLen);

	uint8_t chunk[32768];
	int ret;
	do {
		strm.next_out  = chunk;
		strm.avail_out = static_cast<uint32_t>(sizeof(chunk));
		ret = zng_inflate(&strm, Z_NO_FLUSH);
		if (ret != Z_OK && ret != Z_STREAM_END) {
			zng_inflateEnd(&strm);
			return false;
		}
		size_t produced = sizeof(chunk) - strm.avail_out;
		out.insert(out.end(), chunk, chunk + produced);
	} while (ret != Z_STREAM_END);

	zng_inflateEnd(&strm);
	return true;
}

static bool zlibInflateKnown(uint8_t const* src, size_t srcLen,
                             std::vector<uint8_t>& out, size_t expectedSize) {
	out.resize(expectedSize);
	size_t destLen = expectedSize;
	int ret = zng_uncompress(out.data(), &destLen, src, srcLen);
	if (ret != Z_OK) { out.clear(); return false; }
	out.resize(destLen);
	return true;
}

static uint32_t zlibCrc32(uint8_t const* data, size_t size) {
	return static_cast<uint32_t>(zng_crc32_z(0, data, size));
}

static size_t entryRecordSize(DATArchiveEntry const& entry) {
	std::wstring wpath = utf8::to_wstring(entry.path);
	return wpath.size() * sizeof(wchar_t)
		+ sizeof(uint32_t)
		+ sizeof(uint8_t)
		+ sizeof(uint32_t) * 4
		+ sizeof(uint8_t) * 2;
}

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

static bool readEntryRecord(uint8_t const*& cursor, uint8_t const* end, DATArchiveEntry& entry) {
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
	if (!read(&entry.crc32Value, sizeof(entry.crc32Value))) return false;
	return true;
}

static void collectParentDirectories(std::string_view const& filePath,
                                     std::set<std::string, std::less<>>& dirs) {
	size_t pos = 0;
	while ((pos = filePath.find('/', pos)) != std::string_view::npos) {
		dirs.emplace(filePath.substr(0, pos + 1));
		++pos;
	}
}

static bool isRangeInside(size_t offset, uint32_t relativeOffset, uint32_t size, size_t totalSize) {
	size_t const begin = offset + static_cast<size_t>(relativeOffset);
	if (begin < offset || begin > totalSize) {
		return false;
	}
	size_t const end = begin + static_cast<size_t>(size);
	return end >= begin && end <= totalSize;
}

static bool streamPosToU32(std::streampos pos, uint32_t& out) {
	auto const value = static_cast<std::streamoff>(pos);
	if (value < 0) {
		return false;
	}
	if (static_cast<uintmax_t>(value) > std::numeric_limits<uint32_t>::max()) {
		return false;
	}
	out = static_cast<uint32_t>(value);
	return true;
}

} // anonymous namespace

FileSystemDATArchive::~FileSystemDATArchive() {
	if (m_file.is_open()) m_file.close();
}

bool FileSystemDATArchive::open(std::string_view const& path, size_t readOffset) {
	std::lock_guard lock(m_mutex);

	m_path       = path;
	m_readOffset = readOffset;
	m_entries.clear();
	m_directories.clear();
	m_keyBase = 0;
	m_keyStep = 0;

	std::string pathStr(path);
	m_file.open(pathStr, std::ios::binary);
	if (!m_file.is_open()) {
		Logger::error("FileSystemDATArchive: cannot open '{}'", path);
		return false;
	}

	m_file.seekg(0, std::ios::end);
	auto const archiveEnd = m_file.tellg();
	if (archiveEnd < 0) {
		Logger::error("FileSystemDATArchive: failed to query size of '{}'", path);
		return false;
	}
	auto const archiveSize = static_cast<size_t>(archiveEnd);
	if (m_readOffset > archiveSize || archiveSize - m_readOffset < sizeof(DATArchiveHeader)) {
		Logger::error("FileSystemDATArchive: '{}' is too small for a DAT archive", path);
		return false;
	}

	constexpr std::string_view masterKey{ DATArchiveEncryption::ENCRYPTION_KEY,
	                                      DATArchiveEncryption::ENCRYPTION_KEY_LEN };
	alignas(DATArchiveHeader) uint8_t headerBuf[sizeof(DATArchiveHeader)];

	m_file.seekg(static_cast<std::streamoff>(m_readOffset), std::ios::beg);
	m_file.read(reinterpret_cast<char*>(headerBuf), sizeof(headerBuf));
	if (!m_file) {
		Logger::error("FileSystemDATArchive: failed to read header from '{}'", path);
		return false;
	}

	uint8_t keyBase, keyStep;
	DATArchiveHeader header{};

	DATArchiveEncryption::getKeyHashHeader(masterKey, keyBase, keyStep);
	{
		uint8_t base = keyBase;
		uint8_t buf[sizeof(DATArchiveHeader)];
		std::memcpy(buf, headerBuf, sizeof(buf));
		DATArchiveEncryption::shiftBlock(buf, sizeof(buf), base, keyStep);
		std::memcpy(&header, buf, sizeof(header));

		if (std::memcmp(header.magic, DATArchiveEncryption::HEADER_MAGIC, 8) == 0) {
			m_keyBase = base;
			m_keyStep = keyStep;
		}
		else {
			Logger::error("FileSystemDATArchive: '{}' is not a valid DAT archive", path);
			return false;
		}
	}

	if (header.headerSize == 0) {
		Logger::error("FileSystemDATArchive: metadata size {} is invalid in '{}'", header.headerSize, path);
		return false;
	}
	if (!isRangeInside(m_readOffset, header.headerOffset, header.headerSize, archiveSize)) {
		Logger::error("FileSystemDATArchive: metadata range is outside '{}'", path);
		return false;
	}

	m_file.clear();
	m_file.seekg(static_cast<std::streamoff>(m_readOffset + header.headerOffset), std::ios::beg);

	std::vector<uint8_t> encMeta(header.headerSize);
	m_file.read(reinterpret_cast<char*>(encMeta.data()), header.headerSize);
	if (!m_file) {
		Logger::error("FileSystemDATArchive: failed to read metadata from '{}'", path);
		return false;
	}

	DATArchiveEncryption::shiftBlock(encMeta.data(), encMeta.size(), m_keyBase, m_keyStep);

	std::vector<uint8_t> metaBuf;
	if (!zlibInflate(encMeta.data(), encMeta.size(), metaBuf)) {
		Logger::error("FileSystemDATArchive: failed to decompress metadata in '{}'", path);
		return false;
	}
	if (header.entryCount > metaBuf.size() / sizeof(uint32_t)) {
		Logger::error("FileSystemDATArchive: entry table is invalid in '{}'", path);
		return false;
	}

	uint8_t const* cursor = metaBuf.data();
	uint8_t const* end    = metaBuf.data() + metaBuf.size();

	for (uint32_t i = 0; i < header.entryCount; ++i) {
		uint32_t recordSize = 0;
		if (cursor + static_cast<ptrdiff_t>(sizeof(uint32_t)) > end) {
			Logger::error("FileSystemDATArchive: truncated metadata at entry {} in '{}'", i, path);
			return false;
		}
		std::memcpy(&recordSize, cursor, sizeof(uint32_t));
		cursor += sizeof(uint32_t);
		if (recordSize == 0 || cursor + static_cast<ptrdiff_t>(recordSize) > end) {
			Logger::error("FileSystemDATArchive: invalid record size at entry {} in '{}'", i, path);
			return false;
		}

		uint8_t const* const recordEnd = cursor + recordSize;
		DATArchiveEntry entry{};
		if (!readEntryRecord(cursor, recordEnd, entry) || cursor != recordEnd) {
			Logger::error("FileSystemDATArchive: failed to parse entry {} in '{}'", i, path);
			return false;
		}
		if (entry.path.empty()
			|| entry.compressionType > DATArchiveEntry::CT_ZLIB
			|| !isRangeInside(m_readOffset, entry.offsetPos, entry.sizeStored, archiveSize)
			|| (entry.compressionType == DATArchiveEntry::CT_NONE && entry.sizeFull != entry.sizeStored)) {
			Logger::error("FileSystemDATArchive: invalid entry '{}' in '{}'", entry.path, path);
			return false;
		}

		collectParentDirectories(entry.path, m_directories);
		auto const [it, inserted] = m_entries.emplace(entry.path, std::move(entry));
		if (!inserted) {
			Logger::error("FileSystemDATArchive: duplicate entry '{}' in '{}'", it->first, path);
			return false;
		}
	}

	if (cursor != end) {
		Logger::error("FileSystemDATArchive: trailing metadata in '{}'", path);
		return false;
	}

	return true;
}

std::string_view FileSystemDATArchive::getArchivePath() { return m_path; }

bool FileSystemDATArchive::setPassword(std::string_view const& /*password*/) {
	return false;
}

bool FileSystemDATArchive::hasNode(std::string_view const& name) {
	std::lock_guard lock(m_mutex);
	if (m_entries.count(name)) return true;
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
	if (name.empty()) return true;
	std::string dir(name);
	if (!dir.empty() && dir.back() != '/') dir += '/';
	return m_directories.count(dir) > 0;
}

bool FileSystemDATArchive::readFile(std::string_view const& name, IData** const data) {
	std::lock_guard lock(m_mutex);
	if (!data) return false;
	*data = nullptr;
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

	uint8_t base = entry.keyBase;
	DATArchiveEncryption::shiftBlock(rawBuf.data(), rawBuf.size(), base, entry.keyStep);

	std::vector<uint8_t> outBuf;
	switch (entry.compressionType) {
	case DATArchiveEntry::CT_NONE:
		outBuf = std::move(rawBuf);
		break;

	case DATArchiveEntry::CT_ZLIB:
	{
		if (entry.sizeFull == 0) {
			outBuf.clear();
			break;
		}
		if (entry.sizeStored == 0
			|| !zlibInflateKnown(rawBuf.data(), rawBuf.size(), outBuf, entry.sizeFull)) {
			Logger::warn("FileSystemDATArchive: inflate failed for '{}'", entry.path);
			return false;
		}
		if (outBuf.size() != entry.sizeFull) {
			Logger::warn("FileSystemDATArchive: size mismatch after inflate for '{}' "
			             "(expected {} got {})",
			             entry.path, entry.sizeFull, outBuf.size());
			return false;
		}
		break;
	}

	default:
		return false;
	}

	if (entry.crc32Value != 0 && !outBuf.empty()) {
		uint32_t actual = zlibCrc32(outBuf.data(), outBuf.size());
		if (actual != entry.crc32Value) {
			Logger::warn("FileSystemDATArchive: CRC mismatch for '{}' "
			             "(expected 0x{:08X}, got 0x{:08X})",
			             entry.path, entry.crc32Value, actual);
		}
	}

	SmartReference<IData> result;
	if (!IData::create(outBuf.size(), result.put())) return false;
	if (!outBuf.empty())
		std::memcpy(result->data(), outBuf.data(), outBuf.size());

	*data = result.detach();
	return true;
}

bool FileSystemDATArchive::createEnumerator(IFileSystemEnumerator** const enumerator,
                                            std::string_view const& directory, bool const recursive) {
	if (!enumerator) {
		return false;
	}
	SmartReference<FileSystemDATArchiveEnumerator> object;
	object.attach(new FileSystemDATArchiveEnumerator(this, directory, recursive));
	*enumerator = object.detach();
	return true;
}

bool FileSystemDATArchive::createFromFile(std::string_view const& path,
                                          IFileSystemArchive** const archive) {
	return createFromFile(path, 0, archive);
}

bool FileSystemDATArchive::createFromFile(std::string_view const& path, size_t readOffset,
                                          IFileSystemArchive** const archive) {
	SmartReference<FileSystemDATArchive> object;
	object.attach(new FileSystemDATArchive());
	if (!object->open(path, readOffset)) return false;
	*archive = object.detach();
	return true;
}

FileSystemDATArchiveEnumerator::FileSystemDATArchiveEnumerator(
		FileSystemDATArchive* const archive,
		std::string_view const&    directory,
		bool const                 recursive)
	: m_archive(archive)
{
	assert(archive != nullptr);

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

	std::lock_guard lock(archive->m_mutex);
	for (auto const& [path, entry] : archive->m_entries) {
		if (isPathMatched(path, dir, recursive)) {
			m_items.push_back({ path, false, entry.sizeFull });
		}
	}
	for (auto const& dirPath : archive->m_directories) {
		if (isPathMatched(dirPath, dir, recursive)) {
			m_items.push_back({ dirPath, true, 0 });
		}
	}
}

FileSystemDATArchiveEnumerator::~FileSystemDATArchiveEnumerator() = default;

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
	return m_archive->readFile(m_items[m_index].name, data);
}

void DATArchiveCreator::addFile(std::string_view const& relativePath) {
	std::string p(relativePath);
	normalizeSlashes(p);
	m_files.push_back(std::move(p));
}

bool DATArchiveCreator::create(std::string_view const& baseDir,
                               std::string_view const& outputPath,
                               StatusCallback   onStatus,
                               ProgressCallback onProgress) {
	if (onProgress) onProgress(0.0f);

	uint8_t headerKeyBase, headerKeyStep;
	DATArchiveEncryption::getKeyHashHeader(
		std::string_view(DATArchiveEncryption::ENCRYPTION_KEY, DATArchiveEncryption::ENCRYPTION_KEY_LEN),
		headerKeyBase, headerKeyStep);

	std::string base(baseDir);
	normalizeSlashes(base);
	if (!base.empty() && base.back() != '/') base.push_back('/');

	std::string tmpPath = std::string(outputPath) + ".tmp";
	std::fstream tmpFile(tmpPath, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
	if (!tmpFile.is_open()) {
		Logger::error("DATArchiveCreator: cannot create temp file '{}'", tmpPath);
		return false;
	}

	if (onStatus) onStatus("Writing header");

	if (m_files.size() > std::numeric_limits<uint32_t>::max()) {
		Logger::error("DATArchiveCreator: too many files for DAT archive");
		std::filesystem::remove(tmpPath);
		return false;
	}

	DATArchiveHeader header{};
	std::memcpy(header.magic, DATArchiveEncryption::HEADER_MAGIC, sizeof(header.magic));
	header.entryCount = static_cast<uint32_t>(m_files.size());
	tmpFile.write(reinterpret_cast<char const*>(&header), sizeof(header));

	if (onProgress) onProgress(0.1f);

	std::vector<DATArchiveEntry> entries;
	entries.reserve(m_files.size());
	float progressStep = m_files.empty() ? 0.0f : (0.75f - 0.10f) / static_cast<float>(m_files.size());

	for (size_t i = 0; i < m_files.size(); ++i) {
		if (onStatus) onStatus(std::format("Processing [{}]", m_files[i]));

		std::string fullPath = base + m_files[i];
		std::ifstream file(fullPath, std::ios::binary);
		if (!file.is_open()) {
			Logger::error("DATArchiveCreator: cannot open '{}'", fullPath);
			std::filesystem::remove(tmpPath);
			return false;
		}

		file.seekg(0, std::ios::end);
		uint32_t fileSize = 0;
		if (!streamPosToU32(file.tellg(), fileSize)) {
			Logger::error("DATArchiveCreator: '{}' is too large for DAT archive", fullPath);
			std::filesystem::remove(tmpPath);
			return false;
		}
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> content(fileSize);
		if (fileSize > 0) {
			file.read(reinterpret_cast<char*>(content.data()), fileSize);
			if (!file) {
				Logger::error("DATArchiveCreator: failed to read '{}'", fullPath);
				std::filesystem::remove(tmpPath);
				return false;
			}
		}
		file.close();

		DATArchiveEntry entry;
		entry.path        = m_files[i];
		entry.sizeFull    = fileSize;
		entry.sizeStored  = fileSize;
		if (!streamPosToU32(tmpFile.tellp(), entry.offsetPos)) {
			Logger::error("DATArchiveCreator: archive offset is too large before '{}'", m_files[i]);
			std::filesystem::remove(tmpPath);
			return false;
		}

		entry.crc32Value  = zlibCrc32(content.data(), fileSize);

		DATArchiveEncryption::getKeyHashFile(m_files[i], headerKeyBase, headerKeyStep,
		                                  entry.keyBase, entry.keyStep);

		if (fileSize >= 0x100) {
			entry.compressionType = DATArchiveEntry::CT_ZLIB;
			std::vector<uint8_t> compressed;
			if (zlibDeflate(content.data(), fileSize, compressed)) {
				if (compressed.size() > std::numeric_limits<uint32_t>::max()) {
					Logger::error("DATArchiveCreator: compressed data is too large for '{}'", fullPath);
					std::filesystem::remove(tmpPath);
					return false;
				}
				entry.sizeStored = static_cast<uint32_t>(compressed.size());
				content          = std::move(compressed);
			}
			else {
				entry.compressionType = DATArchiveEntry::CT_NONE;
				entry.sizeStored      = fileSize;
			}
		}

		if (!content.empty())
			tmpFile.write(reinterpret_cast<char const*>(content.data()), entry.sizeStored);

		entries.push_back(std::move(entry));
		if (onProgress) onProgress(0.1f + progressStep * static_cast<float>(i));
	}

	if (onStatus) onStatus("Writing entries info");
	std::streampos metaBegin = tmpFile.tellp();
	if (!streamPosToU32(metaBegin, header.headerOffset)) {
		Logger::error("DATArchiveCreator: metadata offset is too large");
		std::filesystem::remove(tmpPath);
		return false;
	}
	tmpFile.flush();

	std::vector<uint8_t> metaBuf;
	for (auto const& entry : entries) {
		size_t const recordSize = entryRecordSize(entry);
		if (recordSize > std::numeric_limits<uint32_t>::max()) {
			Logger::error("DATArchiveCreator: entry path is too long '{}'", entry.path);
			std::filesystem::remove(tmpPath);
			return false;
		}
		uint32_t const recSize = static_cast<uint32_t>(recordSize);
		metaBuf.insert(metaBuf.end(),
		               reinterpret_cast<uint8_t const*>(&recSize),
		               reinterpret_cast<uint8_t const*>(&recSize) + sizeof(recSize));
		writeEntryRecord(metaBuf, entry);
	}

	std::vector<uint8_t> compMeta;
	if (!zlibDeflate(metaBuf.data(), metaBuf.size(), compMeta)) {
		Logger::error("DATArchiveCreator: failed to compress metadata");
		std::filesystem::remove(tmpPath);
		return false;
	}
	if (compMeta.size() > std::numeric_limits<uint32_t>::max()) {
		Logger::error("DATArchiveCreator: metadata is too large");
		std::filesystem::remove(tmpPath);
		return false;
	}

	tmpFile.write(reinterpret_cast<char const*>(compMeta.data()), compMeta.size());
	header.headerSize = static_cast<uint32_t>(compMeta.size());

	tmpFile.seekp(static_cast<std::streamoff>(offsetof(DATArchiveHeader, headerOffset)));
	tmpFile.write(reinterpret_cast<char const*>(&header.headerOffset), sizeof(uint32_t));
	tmpFile.write(reinterpret_cast<char const*>(&header.headerSize),   sizeof(uint32_t));
	tmpFile.flush();

	if (onStatus) onStatus("Encrypting archive");
	if (onProgress) onProgress(0.95f);

	bool ok = encryptArchive(tmpFile, outputPath, header, headerKeyBase, headerKeyStep, entries);
	tmpFile.close();
	std::filesystem::remove(tmpPath);

	if (!ok) {
		Logger::error("DATArchiveCreator: encryption pass failed");
		return false;
	}

	if (onStatus) onStatus("Done");
	if (onProgress) onProgress(1.0f);
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

	uint8_t headerBase = keyBase;

	src.read(reinterpret_cast<char*>(buf.data()), sizeof(DATArchiveHeader));
	if (src.gcount() != static_cast<std::streamsize>(sizeof(DATArchiveHeader))) {
		return false;
	}
	DATArchiveEncryption::shiftBlock(buf.data(), sizeof(DATArchiveHeader), headerBase, keyStep);
	dest.write(reinterpret_cast<char const*>(buf.data()), sizeof(DATArchiveHeader));
	if (!dest) {
		return false;
	}

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
			if (got != toRead) {
				return false;
			}

			DATArchiveEncryption::shiftBlock(buf.data(), got, localBase, entry.keyStep);
			dest.write(reinterpret_cast<char const*>(buf.data()), static_cast<std::streamsize>(got));
			if (!dest) {
				return false;
			}

			remaining -= got;
		}
	}

	{
		size_t remaining = header.headerSize;

		src.clear();
		src.seekg(static_cast<std::streamoff>(header.headerOffset), std::ios::beg);
		dest.seekp(static_cast<std::streamoff>(header.headerOffset), std::ios::beg);

		while (remaining > 0) {
			size_t toRead = std::min(remaining, CHUNK);
			src.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(toRead));
			size_t got = static_cast<size_t>(src.gcount());
			if (got != toRead) {
				return false;
			}

			DATArchiveEncryption::shiftBlock(buf.data(), got, headerBase, keyStep);
			dest.write(reinterpret_cast<char const*>(buf.data()), static_cast<std::streamsize>(got));
			if (!dest) {
				return false;
			}

			remaining -= got;
		}
	}

	dest.close();
	return !dest.fail();
}

} // namespace core
