#ifndef _INPUT_HELPER_H_
#define _INPUT_HELPER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

#include "XGD.h"
#include "TitleHelper/TitleHelper.h"
#include "InputHelper/Types.h"

class InputHelper
{
public:
    InputHelper(std::filesystem::path in_path, std::filesystem::path out_directory, OutputSettings output_settings);

    void process();
    std::vector<std::filesystem::path> failed_inputs();

private:
    struct InputInfo {
        FileType in_file_type;
        std::vector<std::filesystem::path> in_paths;
    };

    std::vector<InputInfo> input_infos_;
    OutputSettings output_settings_;
    std::filesystem::path output_directory_;
    std::vector<std::filesystem::path> failed_inputs_;

    std::vector<std::filesystem::path> create_image(const InputInfo& input_info);   
    std::vector<std::filesystem::path> create_dir(const InputInfo& input_info);
    std::vector<std::filesystem::path> create_attach_xbe(const InputInfo& input_info);

    bool has_extension(const std::filesystem::path& path, const std::string& extension);
    bool is_extracted_dir(const std::filesystem::path& path);
    bool is_god_dir_helper(const std::filesystem::path& path, int current_depth, int max_depth);
    bool is_god_dir(const std::filesystem::path& path);
    bool is_batch_dir(const std::filesystem::path& path);
    bool is_part_2_file(const std::filesystem::path& path);
    FileType get_filetype(const std::filesystem::path& path);
    std::vector<std::filesystem::path> find_split_filepaths(const std::filesystem::path& in_filepath);
    std::filesystem::path get_output_path(const std::filesystem::path& out_directory, TitleHelper& title_helper);
};

#endif // _INPUT_HELPER_H_