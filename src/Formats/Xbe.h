#ifndef _XBE_H_
#define _XBE_H_

#include <cstdint>

namespace Xbe {
    
// https://github.com/GerbilSoft/rom-properties/blob/fcb6fc09ec7bfbd8b7c6f728aff4308dfa047e2a/src/libromdata/Console/xbox_xbe_structs.h

#pragma pack(push, 1)

    struct Header {
        uint32_t magic;				        // [0x000] 'XBEH'
        uint8_t signature[256];			    // [0x004] RSA-2048 digital signature
        uint32_t base_address;			    // [0x104] Base address (usually 0x00010000)
        uint32_t total_header_size;		    // [0x108] Size of all headers
        uint32_t image_size;			    // [0x10C] Image size
        uint32_t image_header_size;		    // [0x110] Size of the image header
        uint32_t timestamp;			        // [0x114] UNIX timestamp
        uint32_t cert_address;			    // [0x118] Certificate address (in memory)
        uint32_t section_count;			    // [0x11C] Number of sections
        uint32_t section_headers_address;	// [0x120] Address of SectionHeader structs (in memory)
        uint32_t init_flags;			    // [0x124] Initialization flags (See XBE_InitFlags_e.)
        uint32_t entry_point;			    // [0x128] Entry point (XOR'd with Retail or Debug key)
        uint32_t tls_address;			    // [0x12C] TLS address

        // The following fields are taken directly from
        // the original PE executable.
        uint32_t pe_stack_commit;		// [0x130]
        uint32_t pe_heap_reserve;		// [0x134]
        uint32_t pe_heap_commit;		// [0x138]
        uint32_t pe_base_address;		// [0x13C]
        uint32_t pe_size_of_image;		// [0x140]
        uint32_t pe_checksum;			// [0x144]
        uint32_t pe_timestamp;			// [0x148]

        uint32_t debug_pathname_address;	// [0x14C] Address to debug pathname
        uint32_t debug_filename_address;	// [0x150] Address to debug filename
        //         (usually points to the filename
        //          portion of the debug pathname)
        uint32_t debug_filenameW_address;	// [0x154] Address to Unicode debug filename
        uint32_t kernel_thunk_address;		// [0x158] Kernel image thunk address
        //         (XOR'd with Retail or Debug key)

        uint32_t nonkernel_import_dir_address;		// [0x15C]
        uint32_t library_version_count;			    // [0x160]
        uint32_t library_version_address;		    // [0x164]
        uint32_t kernel_library_version_address;	// [0x168]
        uint32_t xapi_library_version_address;		// [0x16C]

        // Logo (usually a Microsoft logo)
        // Encoded using RLE.
        uint32_t logo_bitmap_address;	// [0x170]
        uint32_t logo_bitmap_size;		// [0x174]
    };
    static_assert(sizeof(Header) == 0x178, "Xbe::Header size is not correct");

    struct Cert {
        uint32_t size;				        // [0x000] Size of certificate
        uint32_t timestamp;			        // [0x004] UNIX timestamp
        uint32_t title_id;			        // [0x008] Title ID
        char16_t title_name[40];		    // [0x00C] Title name (UTF-16LE)
        uint32_t alt_title_ids[16];		    // [0x05C] Alternate title IDs
        uint32_t allowed_media_types;	    // [0x09C] Allowed media (bitfield) (see XBE_Media_e)
        uint32_t region_code;			    // [0x0A0] Region code (see XBE_Region_Code_e)
        uint32_t ratings;			        // [0x0A4] Age ratings (TODO)
        uint32_t disc_number;			    // [0x0A8] Disc number
        uint32_t cert_version;			    // [0x0AC] Certificate version
        uint8_t lan_key[16];			    // [0x0B0] LAN key
        uint8_t signature_key[16];		    // [0x0C0] Signature key
        uint8_t alt_signature_keys[16][16];	// [0x0D0] Alternate signature keys
    };
    static_assert(sizeof(Cert) == 0x1D0, "Xbe::Cert size is not correct");

    struct SectionHeader {
        uint32_t flags;	                           // [0x000] Section flags (See XBE_Section_Flags_e)
        uint32_t vaddr;	                           // [0x004] Virtual load address for this section
        uint32_t vsize;	                           // [0x008] Size of this section
        uint32_t paddr;	                           // [0x00C] Physical address in the XBE file
        uint32_t psize;	                           // [0x010] Physical size of this section
        uint32_t section_name_address;             // [0x014] Address of the section name (in memory)
        uint32_t section_name_refcount;	           // [0x018]
        uint32_t head_shared_page_recount_address; // [0x01C]
        uint32_t tail_shared_page_recount_address; // [0x020]
        uint8_t sha1_digest[20];                   // [0x024]
    };
    static_assert(sizeof(SectionHeader) == 56, "Xbe::SectionHeader size is not correct");

#pragma pack(pop)

    struct AllowedMedia {
        static constexpr uint32_t HARD_DISK		= 0x00000001;
        static constexpr uint32_t XGD1			= 0x00000002;
        static constexpr uint32_t DVD_CD        = 0x00000004;
        static constexpr uint32_t CD			= 0x00000008;
        static constexpr uint32_t DVD_5_RO	    = 0x00000010;
        static constexpr uint32_t DVD_9_RO	    = 0x00000020;
        static constexpr uint32_t DVD_5_RW	    = 0x00000040;
        static constexpr uint32_t DVD_9_RW	    = 0x00000080;
        static constexpr uint32_t DONGLE	    = 0x00000100;
        static constexpr uint32_t MEDIA_BOARD   = 0x00000200;
        static constexpr uint32_t NONSECURE_HARD_DISK	= 0x40000000;
        static constexpr uint32_t NONSECURE_MODE		= 0x80000000;

        static constexpr uint32_t ALL = HARD_DISK | XGD1 | DVD_CD | CD | DVD_5_RO | DVD_9_RO | DVD_5_RW | DVD_9_RW | DONGLE | MEDIA_BOARD | NONSECURE_HARD_DISK | NONSECURE_MODE;

        // static constexpr uint32_t MEDIA_MASK    = 0x00FFFFFF;
    };

    static constexpr char     MAGIC[] = "XBEH";
    static constexpr int      MAGIC_LEN  = 4;

};

#endif // _XBE_H_