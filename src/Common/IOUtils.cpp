#include "XGD.h"
#include "Common/IOUtils.h"
#include "Common/EndianUtils.h"

namespace IOUtils {

void pad_to_modulus(split::ofstream& out_file, uint32_t modulus, char pad_byte) {
    if (!out_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Output file is not open");
    }

    std::streampos current_pos = out_file.tellp();
    std::streamoff padding_len = modulus - (current_pos % modulus);

    if (padding_len && padding_len < modulus) {
        std::vector<char> buffer(padding_len, pad_byte);
        out_file.write(buffer.data(), padding_len);
    }

    if (out_file.fail()) {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding bytes");
    }
}

void pad_to_modulus(std::ofstream& out_file, uint32_t modulus, char pad_byte) {
    if (!out_file.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), "Output file is not open");
    }

    std::streampos current_pos = out_file.tellp();
    std::streamoff padding_len = modulus - (current_pos % modulus);

    if (padding_len && padding_len < modulus) {
        std::vector<char> buffer(padding_len, pad_byte);
        out_file.write(buffer.data(), padding_len);
    }

    if (out_file.fail()) {
        throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write padding bytes");
    }
}

bool get_filetime(FileTime &file_time) {
	double tmp;
	time_t now;

	if ((now = std::time(nullptr)) == -1) {
        return false;
    }

    tmp = ((double) now + (369.0 * 365.25 * 24 * 60 * 60 - (3.0 * 24 * 60 * 60 + 6.0 * 60 * 60))) * 1.0e7;

    file_time.high = static_cast<uint32_t>(tmp * (1.0 / (4.0 * (double) (1 << 30))));
    file_time.low =  static_cast<uint32_t>(tmp - ((double) file_time.high) * 4.0 * (double) (1 << 30));
    
    EndianUtils::little_32(file_time.high);
    EndianUtils::little_32(file_time.low);

    return true;
}

}; // namespace IOUtils