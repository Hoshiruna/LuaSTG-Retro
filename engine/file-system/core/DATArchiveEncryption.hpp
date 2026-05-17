#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace core {

class DATArchiveEncryption {
public:
	static constexpr char const HEADER_MAGIC[]       = "LSTGRETROARC\0\0";
	static constexpr size_t     HEADER_MAGIC_LENGTH  = 8;

	static constexpr char const ENCRYPTION_KEY[]    = "Sonic The Hedgehog";
	static constexpr size_t     ENCRYPTION_KEY_LEN  = sizeof(ENCRYPTION_KEY) - 1;

	static constexpr uint32_t fnv1a_32(char const* data, size_t len) {
		constexpr uint32_t OFFSET_BASIS = 2166136261u;
		constexpr uint32_t FNV_PRIME    = 16777619u;
		uint32_t hash = OFFSET_BASIS;
		for (size_t i = 0; i < len; ++i) {
			hash ^= static_cast<uint8_t>(data[i]);
			hash *= FNV_PRIME;
		}
		return hash;
	}
	static constexpr uint32_t fnv1a_32(std::string_view const& s) {
		return fnv1a_32(s.data(), s.size());
	}

	static void getKeyHashHeader(std::string_view const& key,
	                             uint8_t& keyBase, uint8_t& keyStep) {
		uint32_t hash = fnv1a_32(key);
		keyBase = static_cast<uint8_t>( hash        & 0xFF) ^ 0x55;
		keyStep = static_cast<uint8_t>((hash >> 8) & 0xFF) ^ 0xC8;
	}

	static void getKeyHashFile(std::string_view const& path,
	                           uint8_t headerBase, uint8_t headerStep,
	                           uint8_t& keyBase,   uint8_t& keyStep) {
		uint32_t hash = fnv1a_32(path);
		keyBase = static_cast<uint8_t>((hash >> 24) & 0xFF) ^ headerBase ^ 0x4A;
		keyStep = static_cast<uint8_t>((hash >> 16) & 0xFF) ^ headerStep ^ 0xEB;
	}

	static void shiftBlock(uint8_t* data, size_t count, uint8_t& base, uint8_t step) {
		for (size_t i = 0; i < count; ++i) {
			data[i] ^= base;
			base = static_cast<uint8_t>(
				static_cast<uint32_t>(base) * 0xBD + static_cast<uint32_t>(step));
		}
	}
};

} // namespace core
