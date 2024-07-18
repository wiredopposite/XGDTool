#include <algorithm>

#include "SplitFStream/SplitFStream.h"

split::ifstream::ifstream(ifstream&& other) noexcept
    : infiles(std::move(other.infiles)),
      current_stream(other.current_stream),
      current_position(other.current_position),
      end_of_file(other.end_of_file),
      total_size(other.total_size),
      last_gcount(other.last_gcount) {}

split::ifstream& split::ifstream::operator=(ifstream&& other) noexcept {
    if (this != &other) {
        infiles = std::move(other.infiles);
        current_stream = other.current_stream;
        current_position = other.current_position;
        end_of_file = other.end_of_file;
        total_size = other.total_size;
        last_gcount = other.last_gcount;
    }
    return *this;
}

split::ifstream::ifstream(const std::vector<std::filesystem::path> &_Paths) {
    infiles.resize(_Paths.size());
    for (size_t i = 0; i < _Paths.size(); i++) {
        infiles[i].path = _Paths[i];
    }
    init_streams();
}

split::ifstream::ifstream(const std::vector<std::string> &_Paths) {
    infiles.resize(_Paths.size());
    for (size_t i = 0; i < _Paths.size(); i++) {
        infiles[i].path = std::filesystem::absolute(_Paths[i]);
    }
    init_streams();
}

split::ifstream::ifstream(const std::filesystem::path &_Path) {
    infiles.resize(1);
    infiles[0].path = _Path;
    init_streams();
}

split::ifstream::~ifstream() {
    close();
}

void split::ifstream::push_back(const std::filesystem::path &_Path) {
    StreamInfo new_stream_info = { 
        std::ifstream(_Path, std::ios::binary), 
        std::filesystem::file_size(_Path), 
        std::filesystem::absolute(_Path) 
    };
    infiles.push_back(std::move(new_stream_info));
    total_size += infiles.back().size;
    end_of_file = false;
}

uint64_t split::ifstream::size() const {
    return total_size;
}

void split::ifstream::init_streams() {
    for (auto& file : infiles) {
        file.stream.open(file.path, std::ios::binary);
        if (!file.stream.is_open()) {
            throw std::runtime_error("Failed to open file: " + file.path.string());
        }

        file.size = std::filesystem::file_size(file.path);
        total_size += file.size;
    }
}

void split::ifstream::seekg(uint64_t _Off, std::ios_base::seekdir _Way) {
    uint64_t new_position;

    switch (_Way) {
        case std::ios_base::beg:
            new_position = _Off;
            break;
        case std::ios_base::cur:
            new_position = current_position + _Off;
            break;
        case std::ios_base::end:
            new_position = total_size + _Off;
            break;
        default:
            throw std::invalid_argument("Invalid seek direction");
    }

    if (new_position > total_size) {
        for (auto& stream_info : infiles) {
            stream_info.stream.setstate(std::ios::failbit);
        }
        current_position = total_size;
    } else {
        current_position = new_position;
    }

    uint64_t bytes_left = current_position;

    if (bytes_left == total_size) {
        current_stream = static_cast<unsigned int>(infiles.size() - 1);
        infiles[current_stream].stream.seekg(0, std::ios_base::end);
        end_of_file = infiles[current_stream].stream.eof();
        return;
    }

    for (current_stream = 0; current_stream < infiles.size(); ++current_stream) {
        if (bytes_left <= infiles[current_stream].size) {
            infiles[current_stream].stream.seekg(bytes_left, std::ios::beg);
            end_of_file = false;
            return;
        }
        bytes_left -= infiles[current_stream].size;
    }
}

split::ifstream& split::ifstream::read(char* _Str, std::streamsize _Count) {
    uint64_t bytes_to_read = _Count;
    last_gcount = 0;

    while (bytes_to_read > 0) {
        uint64_t bytes_left_in_stream = infiles[current_stream].size - infiles[current_stream].stream.tellg();
        
        if (bytes_left_in_stream >= bytes_to_read) {
            // The current stream has enough bytes to fulfill the read request
            infiles[current_stream].stream.read(_Str, bytes_to_read);
            last_gcount += infiles[current_stream].stream.gcount();
            current_position += last_gcount;
            return *this;
        } else {
            // Current stream doesn't have enough bytes
            infiles[current_stream].stream.read(_Str, bytes_left_in_stream);
            last_gcount += infiles[current_stream].stream.gcount();
            current_position += bytes_left_in_stream;
            _Str += bytes_left_in_stream;
            bytes_to_read -= bytes_left_in_stream;
            current_stream++;

            if (current_stream >= infiles.size()) {
                end_of_file = true;
                return *this;
            }

            infiles[current_stream].stream.seekg(0, std::ios::beg);
        }
    }
    return *this;
}

uint64_t split::ifstream::tellg() {
    return current_position;
}

uint64_t split::ifstream::gcount() const {
    return last_gcount;
}

bool split::ifstream::eof() const {
    return end_of_file;
}

bool split::ifstream::operator!() {
    return !good();
}

bool split::ifstream::is_open() const {
    return std::all_of(infiles.begin(), infiles.end(), [](const StreamInfo& si) { return si.stream.is_open(); });
}

bool split::ifstream::fail() const {
    return std::any_of(infiles.begin(), infiles.end(), [](const StreamInfo& si) { return si.stream.fail(); });
}

bool split::ifstream::bad() const {
    return std::any_of(infiles.begin(), infiles.end(), [](const StreamInfo& si) { return si.stream.bad(); });
}

bool split::ifstream::good() const {
    return std::all_of(infiles.begin(), infiles.end(), [](const StreamInfo& si) { return si.stream.good(); });
}

void split::ifstream::clear() {
    for (auto& file : infiles) {
        file.stream.clear();
    }
}

void split::ifstream::close() {
    for (auto& file : infiles) {
        if (file.stream.is_open()) {
            file.stream.close();
        }
    }
    infiles.clear();
    current_stream = 0;
    current_position = 0;
    last_gcount = 0;
    end_of_file = false;
}