#ifndef _XEX_H_
#define _XEX_H_

#include <cstdint>

// https://github.com/emoose/idaxex

namespace Xex {

#pragma pack(push, 1)

	struct Header {
		uint32_t magic;
		uint32_t module_flags;
		uint32_t sizeof_headers;
		uint32_t sizeof_discardable_headers;
		uint32_t security_info;
		uint32_t header_count;
	};
	static_assert(sizeof(Header) == 0x18, "Xex::Header");

	struct DirectoryEntry {
		uint32_t key;
		uint32_t value;
	};
	static_assert(sizeof(DirectoryEntry) == 8, "Xex::DirectoryEntry");

	// struct ExecutionInfo {
	// 	uint32_t media_id;
	// 	union {
	// 		uint32_t version;
	// 		struct {
	// 			uint32_t major   : 4;
	// 			uint32_t minor   : 4;
	// 			uint32_t build   : 16;
	// 			uint32_t qfe     : 8;
	// 		};
	// 	};
	// 	union {
	// 		uint32_t base_version;
	// 		struct {
	// 			uint32_t major   : 4;
	// 			uint32_t minor   : 4;
	// 			uint32_t build   : 16;
	// 			uint32_t qfe     : 8;
	// 		};
	// 	};
	// 	union {
	// 		uint32_t title_id;
	// 		struct {
	// 			uint16_t publisher_id;
	// 			uint16_t game_id;
	// 		};
	// 	};
	// 	uint8_t platform;
	// 	uint8_t executable_type;
	// 	uint8_t disc_number;
	// 	uint8_t disc_count;
	// 	uint32_t savegame_id;
	// };
	struct ExecutionInfo {
		uint32_t media_id;
		union {
			uint32_t version;
			struct {
				uint32_t version_major   : 4;
				uint32_t version_minor   : 4;
				uint32_t version_build   : 16;
				uint32_t version_qfe     : 8;
			};
		};
		union {
			uint32_t base_version;
			struct {
				uint32_t base_major   : 4;
				uint32_t base_minor   : 4;
				uint32_t base_build   : 16;
				uint32_t base_qfe     : 8;
			};
		};
		union {
			uint32_t title_id;
			struct {
				uint16_t publisher_id;
				uint16_t game_id;
			};
		};
		uint8_t platform;
		uint8_t executable_type;
		uint8_t disc_number;
		uint8_t disc_count;
		uint32_t savegame_id;
	};
	static_assert(sizeof(ExecutionInfo) == 24, "Xex:ExecutionInfo");

#pragma pack(pop)

	namespace KeyValue {
		constexpr uint32_t RESOURCE_INFO                 = 0x000002FF;
		constexpr uint32_t FILE_FORMAT_INFO              = 0x000003FF;
		constexpr uint32_t DELTA_PATCH_DESCRIPTOR        = 0x000005FF;
		constexpr uint32_t BASE_REFERENCE                = 0x00000405;
		constexpr uint32_t BOUNDING_PATH                 = 0x000080FF;
		constexpr uint32_t DEVICE_ID                     = 0x00008105;
		constexpr uint32_t ORIGINAL_BASE_ADDRESS         = 0x00010001;
		constexpr uint32_t ENTRY_POINT                   = 0x00010100;
		constexpr uint32_t IMAGE_BASE_ADDRESS            = 0x00010201;
		constexpr uint32_t IMPORT_LIBRARIES              = 0x000103FF;
		constexpr uint32_t CHECKSUM_TIMESTAMP            = 0x00018002;
		constexpr uint32_t ENABLED_FOR_CALLCAP           = 0x00018102;
		constexpr uint32_t ENABLED_FOR_FASTCAP           = 0x00018200;
		constexpr uint32_t ORIGINAL_PE_NAME              = 0x000183FF;
		constexpr uint32_t STATIC_LIBRARIES              = 0x000200FF;
		constexpr uint32_t TLS_INFO                      = 0x00020104;
		constexpr uint32_t DEFAULT_STACK_SIZE            = 0x00020200;
		constexpr uint32_t DEFAULT_FILESYSTEM_CACHE_SIZE = 0x00020301;
		constexpr uint32_t DEFAULT_HEAP_SIZE             = 0x00020401;
		constexpr uint32_t PAGE_HEAP_SIZE_AND_FLAGS      = 0x00028002;
		constexpr uint32_t SYSTEM_FLAGS                  = 0x00030000;
		constexpr uint32_t EXECUTION_INFO                = 0x00040006;
		constexpr uint32_t TITLE_WORKSPACE_SIZE          = 0x00040201;
		constexpr uint32_t GAME_RATINGS                  = 0x00040310;
		constexpr uint32_t LAN_KEY                       = 0x00040404;
		constexpr uint32_t XBOX360_LOGO                  = 0x000405FF;
		constexpr uint32_t MULTIDISC_MEDIA_IDS           = 0x000406FF;
		constexpr uint32_t ALTERNATE_TITLE_IDS           = 0x000407FF;
		constexpr uint32_t ADDITIONAL_TITLE_MEMORY       = 0x00040801;
		constexpr uint32_t EXPORTS_BY_NAME               = 0x00E10402;
	};

}; // namespace Xex

#endif // _XEX_H_