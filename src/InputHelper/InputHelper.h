#ifndef _INPUT_HELPER_H_
#define _INPUT_HELPER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

#include "XGD.h"
#include "TitleHelper/TitleHelper.h"
#include "InputHelper/Types.h"
#include "ZARExtractor/ZARExtractor.h"
#include "ImageWriter/ImageWriter.h"
#include "ImageExtractor/ImageExtractor.h"    

class InputHelper
{
public:
    struct InputInfo 
    {
        FileType file_type;
        std::vector<std::filesystem::path> paths;
    };

    InputHelper(std::filesystem::path in_path, std::filesystem::path out_directory, OutputSettings output_settings);
    InputHelper(std::vector<std::filesystem::path> in_paths, std::filesystem::path out_directory, OutputSettings output_settings);

    const std::vector<InputInfo>& input_infos() { return input_infos_; };

    void process_all();
    void process_single(InputInfo input_info);

    void cancel_processing();
    void pause_processing();
    void resume_processing();

    //Return failed input paths after processing or canceling
    const std::vector<std::filesystem::path>& failed_inputs() { return failed_inputs_; }; 

private:
    std::vector<InputInfo> input_infos_;
    OutputSettings output_settings_;
    std::filesystem::path output_directory_;
    std::vector<std::filesystem::path> failed_inputs_;

    // Instances stored here so cancel flag can be set asynchrnously
    std::unique_ptr<ImageWriter> image_writer_{nullptr};
    std::unique_ptr<ImageExtractor> image_extractor_{nullptr};
    std::unique_ptr<ZARExtractor> zar_extractor_{nullptr};

    std::vector<std::filesystem::path> create_image(InputInfo& input_info);
    std::vector<std::filesystem::path> create_dir(const InputInfo& input_info);
    std::vector<std::filesystem::path> create_attach_xbe(const InputInfo& input_info);
    void list_files(const InputInfo& input_info);
    std::filesystem::path extract_temp_zar(const std::filesystem::path& in_path);
    
    void add_input(const std::filesystem::path& in_path);
    bool has_extension(const std::filesystem::path& path, const std::string& extension);
    bool is_extracted_dir(const std::filesystem::path& path);
    bool is_god_dir_helper(const std::filesystem::path& path, int current_depth, int max_depth);
    bool is_god_dir(const std::filesystem::path& path);
    bool is_batch_dir(const std::filesystem::path& path);
    bool is_part_2_file(const std::filesystem::path& path);
    FileType get_filetype(const std::filesystem::path& path);

    void remove_duplicate_infos(std::vector<InputInfo>& input_infos);
    OutputSettings get_auto_output_settings(const AutoFormat auto_format);
    std::vector<std::filesystem::path> find_split_filepaths(const std::filesystem::path& in_filepath);
    std::filesystem::path get_output_path(const std::filesystem::path& out_directory, TitleHelper& title_helper);
    void reset_processor();
};

#endif // _INPUT_HELPER_H_