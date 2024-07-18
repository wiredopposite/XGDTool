#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "SplitFStream/SplitFStream.h"

split::ofstream::ofstream(ofstream&& other) noexcept
    : outfiles(std::move(other.outfiles)),
      parent_path(std::move(other.parent_path)),
      file_stem(std::move(other.file_stem)),
      file_ext(std::move(other.file_ext)),
      max_filesize(other.max_filesize),
      current_stream(other.current_stream),
      current_position(other.current_position) {
}

split::ofstream& split::ofstream::operator=(ofstream&& other) noexcept {
    if (this != &other) {
        close();
        outfiles = std::move(other.outfiles);
        parent_path = std::move(other.parent_path);
        file_stem = std::move(other.file_stem);
        file_ext = std::move(other.file_ext);
        max_filesize = other.max_filesize;
        current_stream = other.current_stream;
        current_position = other.current_position;
    }
    return *this;
}

split::ofstream::ofstream(const std::filesystem::path &_Path, const uint64_t &_Maxsize)
    :   parent_path(_Path.parent_path()),
        file_stem(_Path.stem().string()), 
        file_ext(_Path.extension().string()),
        max_filesize(_Maxsize) {
    open_new_stream();
}

split::ofstream::~ofstream() {
    close();
    clean_all();
}

split::ofstream& split::ofstream::seekp(uint64_t _Off, std::ios_base::seekdir _Way) {
    uint64_t new_pos = 0;
    switch (_Way) {
        case std::ios_base::beg:
            new_pos = _Off;
            break;
        case std::ios_base::cur:
            new_pos = current_position + _Off;
            break;
        case std::ios_base::end: {
            uint64_t back_position = outfiles.back().stream.tellp();
            outfiles.back().stream.seekp(0, std::ios::end);
            uint64_t total_size = (outfiles.size() - 1) * max_filesize + outfiles.back().stream.tellp();
            outfiles.back().stream.seekp(back_position, std::ios::beg);
            new_pos = total_size + _Off;
            if (new_pos > total_size) {
                new_pos = total_size;
            }
            break;
        }
        default:
            throw std::invalid_argument("Invalid seek direction");
    }

    if (new_pos > current_position) {
        while (current_stream < outfiles.size() && new_pos > max_filesize * (current_stream + 1)) {
            ++current_stream;
        }
    } else {
        while (current_stream > 0 && new_pos <= max_filesize * current_stream) {
            --current_stream;
        }
    }

    current_position = new_pos;
    uint64_t pos_in_file = current_position - max_filesize * current_stream;
    outfiles[current_stream].stream.seekp(pos_in_file, std::ios::beg);

    return *this;
}

split::ofstream& split::ofstream::write(const char* _Str, std::streamsize _Count) {
    while (_Count > 0) {
        uint64_t bytes_left = max_filesize - outfiles[current_stream].stream.tellp();

        if (bytes_left <= 0) {
            current_stream++;
            if (current_stream >= outfiles.size()) {
                open_new_stream();
            }
            continue;
        }

        std::streamsize to_write = std::min(static_cast<uint64_t>(_Count), bytes_left);
        outfiles[current_stream].stream.write(_Str, to_write);
        _Str += to_write;
        _Count -= to_write;
        current_position += to_write;
    }
    return *this;
}

uint64_t split::ofstream::tellp() {
    return current_position;
}

bool split::ofstream::operator!() {
    return !good();
}

bool split::ofstream::is_open() const {
    return std::any_of(outfiles.begin(), outfiles.end(), [](const StreamInfo& si) { return si.stream.is_open(); });
}

bool split::ofstream::fail() const {
    return std::any_of(outfiles.begin(), outfiles.end(), [](const StreamInfo& si) { return si.stream.fail(); });
}

bool split::ofstream::bad() const {
    return std::any_of(outfiles.begin(), outfiles.end(), [](const StreamInfo& si) { return si.stream.bad(); });
}

bool split::ofstream::good() const {
    return std::all_of(outfiles.begin(), outfiles.end(), [](const StreamInfo& si) { return si.stream.good(); });
}

void split::ofstream::clear() {
    for (auto& file : outfiles) {
        file.stream.clear();
    }
}

void split::ofstream::close() {
    for (auto& file : outfiles) {
        file.stream.close();
    }
    rename_output_files();
}

void split::ofstream::clean_all() {
    for (auto& file : outfiles) {
        if (file.stream.is_open()) {
            file.stream.close();
        }
    }
    outfiles.clear();
    current_stream = 0;
    current_position = 0;
    max_filesize = UINT64_MAX;
    parent_path.clear();
    file_stem.clear();
    file_ext.clear();
}

std::filesystem::path split::ofstream::get_next_filepath() {
    return parent_path / (file_stem + "." + std::to_string(current_stream + 1) + file_ext);
}

void split::ofstream::open_new_stream() {
    std::filesystem::path filepath = get_next_filepath();
    outfiles.push_back({ std::ofstream(filepath, std::ios::binary), filepath, current_stream });
}

void split::ofstream::rename_output_files() {
    if (outfiles.size() > 1 && outfiles.size() < 10) {
        return;
    } else if (outfiles.size() == 1) {
        try {
            std::filesystem::rename(outfiles[0].path, parent_path / (file_stem + file_ext));
            outfiles[0].path = parent_path / (file_stem + file_ext);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        return;
    }

    int digits = num_digits(static_cast<int>(outfiles.size()));

    for (auto& file : outfiles) {
        if (!std::filesystem::exists(file.path)) {
            continue;
        }

        bool err = false;
        std::filesystem::path new_path = parent_path / (file_stem + "." + pad_digits(file.index + 1, digits) + file_ext);

        try {
            std::filesystem::rename(file.path, new_path);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            err = true;
        }
        if (!err) {
            file.path = new_path;
        }
    }
}

int split::ofstream::num_digits(int number) {
    if (number == 0) {
        return 1;
    }
    return static_cast<int>(std::log10(std::abs(number))) + 1;
}

std::string split::ofstream::pad_digits(int number, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << number;
    return oss.str();
}

split::PathsWrapper split::ofstream::paths() const {
    std::vector<std::filesystem::path> paths;
    for (const auto& file : outfiles) {
        paths.push_back(file.path);
    }
    return PathsWrapper(paths);
}

std::vector<std::string> split::PathsWrapper::string() const {
    std::vector<std::string> str_paths;
    str_paths.reserve(paths_.size());
    for (const auto& path : paths_) {
        str_paths.push_back(path.string());
    }
    return str_paths;
}