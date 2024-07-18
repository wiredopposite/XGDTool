#ifndef _SPLIT_FSTREAM_H_
#define _SPLIT_FSTREAM_H_

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>

namespace split {

class PathsWrapper {
public:
    explicit PathsWrapper(const std::vector<std::filesystem::path>& paths) : paths_(paths) {}
    operator std::vector<std::filesystem::path>() const { return paths_; }
    std::vector<std::string> string() const;

private:
    std::vector<std::filesystem::path> paths_;
};

class ofstream {
public:
    ofstream() {};
    ofstream(const std::filesystem::path &_Path, const uint64_t &_Maxsize = UINT64_MAX);
    ofstream(ofstream&& other) noexcept;
    ofstream& operator=(ofstream&& other) noexcept;
    ~ofstream();

    bool operator!();

    ofstream& seekp(uint64_t _Off, std::ios_base::seekdir _Way);
    ofstream& write(const char* _Str, std::streamsize _Count);
    uint64_t tellp();

    bool is_open() const;
    bool fail() const;
    bool bad() const;
    bool good() const;
    void close();
    void clear();

    PathsWrapper paths() const;
    
private:
    struct StreamInfo {
        std::ofstream stream;
        std::filesystem::path path;
        unsigned int index;
    };

    std::vector<StreamInfo> outfiles;

    std::string file_stem;
    std::string file_ext;
    std::filesystem::path parent_path;

    unsigned int current_stream{0};
    uint64_t current_position{0};
    uint64_t max_filesize;
    
    std::filesystem::path get_next_filepath();
    void open_new_stream();
    void rename_output_files();
    int num_digits(int number);
    std::string pad_digits(int number, int width);
    void clean_all();
};

class ifstream {
public:
    ifstream() {};
    ifstream(ifstream&& other) noexcept;
    ifstream& operator=(ifstream&& other) noexcept;
    ifstream(const std::vector<std::string> &_Paths);
    ifstream(const std::vector<std::filesystem::path> &_Paths);
    ifstream(const std::filesystem::path &_Path);
    ~ifstream();

    bool operator!();

    void push_back(const std::filesystem::path &_Path);
    uint64_t size() const;

    void close();
    void clear();
    uint64_t tellg();
    uint64_t gcount() const;
    void seekg(uint64_t _Off, std::ios_base::seekdir _Way);
    ifstream& read(char* _Str, std::streamsize _Count);

    bool is_open() const;
    bool eof() const;
    bool fail() const;
    bool bad() const;
    bool good() const;
    
private:
    void init_streams();

    struct StreamInfo {
        std::ifstream stream;
        uint64_t size;
        std::filesystem::path path;
    };

    std::vector<StreamInfo> infiles;
    uint64_t total_size{0};
    unsigned int current_stream{0};
    uint64_t current_position{0};
    uint64_t last_gcount{0};
    bool end_of_file{false};
};

}; // namespace split

#endif // _SPLIT_FSTREAM_H_