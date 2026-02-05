#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace core {

// Encryption and key-derivation primitives for the LSTGRETROARC archive format.
// The cipher is a byte-at-a-time XOR stream; encryption and decryption are the
// same operation.  Two variants exist:
//   v2 (current)  – base advances as  base = base * 0xBD + step  (mod 256)
//   v1 (legacy)   – base advances as  base = (base + step)       (mod 256)
class ArchiveEncryption {
public:
	// ── format constants ──────────────────────────────────────────────────
	// Full identifier written into the archive.  Only the first HEADER_MAGIC_LENGTH
	// bytes are stored in the on-disk header struct.
	static constexpr char const HEADER_MAGIC[]       = "LSTGRETROARC\0\0";
	static constexpr size_t     HEADER_MAGIC_LENGTH  = 8;

	static constexpr uint32_t VERSION_CURRENT = 2;
	static constexpr uint32_t VERSION_LEGACY  = 1;

	// Master encryption key.  Change to whatever you like before shipping.
	static constexpr char const ENCRYPTION_KEY[]    = "Sonic The Hedgehog";
	static constexpr size_t     ENCRYPTION_KEY_LEN  = sizeof(ENCRYPTION_KEY) - 1;

	// ── hashing ───────────────────────────────────────────────────────────
	// FNV-1a 32-bit.  Constexpr so it can be used in compile-time contexts.
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

	// ── key derivation ────────────────────────────────────────────────────
	// Header-level key (v2).  Feeds the master encryption key through FNV-1a
	// and extracts two bytes for the stream-cipher seed.
	static void getKeyHashHeader(std::string_view const& key,
	                             uint8_t& keyBase, uint8_t& keyStep) {
		uint32_t hash = fnv1a_32(key);
		keyBase = static_cast<uint8_t>( hash        & 0xFF) ^ 0x55;
		keyStep = static_cast<uint8_t>((hash >> 8) & 0xFF) ^ 0xC8;
	}

	// Header-level key (v1 / legacy).  Uses the MSVC runtime hash so that
	// archives produced by the original engine remain readable.
	// Falls back to FNV-1a on non-MSVC (v1 archives will not decrypt correctly there).
	static void getKeyHashHeaderLegacy(std::string_view const& key,
	                                   uint8_t& keyBase, uint8_t& keyStep) {
#ifdef _MSC_VER
		uint32_t hash = static_cast<uint32_t>(
			std::_Hash_array_representation(key.data(), key.size()));
#else
		uint32_t hash = fnv1a_32(key);
#endif
		keyBase = static_cast<uint8_t>( hash        & 0xFF) ^ 0x55;
		keyStep = static_cast<uint8_t>((hash >> 8) & 0xFF) ^ 0xC8;
	}

	// Per-file key.  Mixes the file path hash with the header-level key so
	// that every file in the archive gets an independent cipher stream.
	static void getKeyHashFile(std::string_view const& path,
	                           uint8_t headerBase, uint8_t headerStep,
	                           uint8_t& keyBase,   uint8_t& keyStep) {
		uint32_t hash = fnv1a_32(path);
		keyBase = static_cast<uint8_t>((hash >> 24) & 0xFF) ^ headerBase ^ 0x4A;
		keyStep = static_cast<uint8_t>((hash >> 16) & 0xFF) ^ headerStep ^ 0xEB;
	}

	// ── stream cipher ─────────────────────────────────────────────────────
	// v2: base ← base * 0xBD + step  (mod 256).
	// `base` is updated in-place so the caller can continue the same stream.
	static void shiftBlock(uint8_t* data, size_t count, uint8_t& base, uint8_t step) {
		for (size_t i = 0; i < count; ++i) {
			data[i] ^= base;
			base = static_cast<uint8_t>(
				static_cast<uint32_t>(base) * 0xBD + static_cast<uint32_t>(step));
		}
	}

	// v1 (legacy): base ← (base + step) mod 256.
	static void shiftBlockLegacy(uint8_t* data, size_t count, uint8_t& base, uint8_t step) {
		for (size_t i = 0; i < count; ++i) {
			data[i] ^= base;
			base = static_cast<uint8_t>(
				static_cast<uint32_t>(base) + static_cast<uint32_t>(step));
		}
	}
};

} // namespace core
