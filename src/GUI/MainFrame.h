#ifndef _MAIN_FRAME_H_
#define _MAIN_FRAME_H_

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

#include <wx/wx.h>
#include <wx/gbsizer.h>
#include <wx/grid.h>
#include <wx/listctrl.h>
#include <wx/hyperlink.h>

#include "XGD.h"
#include "InputHelper/Types.h"
#include "InputHelper/InputHelper.h"

wxDECLARE_EVENT(wxEVT_UPDATE_CURRENT_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_UPDATE_TOTAL_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_THREAD_COMPLETED, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_UPDATE_CURRENT_STAGE, wxThreadEvent);

class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~MainFrame();

    static void update_progress_bar(uint64_t current, uint64_t total);
    static void update_status_field(const std::string status);

private:
    enum class Status { IDLE, PROCESSING, PAUSED, CANCELED };

    struct Picker
    {
        wxTextCtrl* field{nullptr};
        wxButton* button{nullptr};
    };

    struct OutFormatRadioButtons
    {
        wxRadioButton* extract{nullptr};
        wxRadioButton* iso{nullptr};
        wxRadioButton* god{nullptr};
        wxRadioButton* cci{nullptr};
        wxRadioButton* cso{nullptr};
        wxRadioButton* zar{nullptr};
    };

    struct AutoFormatRadioButtons
    {
        wxRadioButton* ogxbox{nullptr};
        wxRadioButton* xbox360{nullptr};
        wxRadioButton* xemu{nullptr};
        wxRadioButton* xenia{nullptr};
    };

    struct ScrubRadioButtons
    {
        wxRadioButton* none{nullptr};
        wxRadioButton* partial{nullptr};
        wxRadioButton* full{nullptr};
    };

    struct SettingsCheckBoxes
    {
        wxCheckBox* split{nullptr};
        wxCheckBox* attach_xbe{nullptr};
        wxCheckBox* allowed_media_xbe{nullptr};
        wxCheckBox* rename_xbe{nullptr};
        wxCheckBox* offline_mode{nullptr};
    };

    struct ProcessButtons
    {
        wxButton* process{nullptr};
        wxButton* pause{nullptr};
        wxButton* cancel{nullptr};
    };

    std::atomic<Status> current_status_{Status::IDLE};
    std::unique_ptr<std::thread> processing_thread_{nullptr};
    std::unique_ptr<InputHelper> input_helper_{nullptr};
    std::vector<std::filesystem::path> input_paths_;
    std::filesystem::path output_path_;

    Picker input_picker_;
    Picker output_picker_;

    wxListCtrl* file_list_{nullptr};

    wxTextCtrl* status_field_{nullptr};
    std::string stored_status_;

    OutFormatRadioButtons out_format_rbs_;
    AutoFormatRadioButtons auto_format_rbs_;
    ScrubRadioButtons out_scrub_rbs_;
    SettingsCheckBoxes out_settings_cbs_;
    ProcessButtons process_buttons_;

    static wxGauge* current_progress_bar_;
    wxGauge* total_progress_bar_{nullptr};

    void on_pick_input_path(wxCommandEvent& event);
    void on_pick_output_path(wxCommandEvent& event);
    void on_process_all(wxCommandEvent& event);
    void on_pause_process(wxCommandEvent& event);
    void on_cancel_process(wxCommandEvent& event);
    void on_update_current_progress(wxThreadEvent& event);
    void on_update_total_progress(wxThreadEvent& event);
    void on_thread_completed(wxThreadEvent& event);
    void on_update_current_stage(wxThreadEvent& event);

    void update_current_progress_bar(uint64_t progress, uint64_t total);
    void process_files();
    void update_controls_state();
    void update_button_states();
    OutputSettings parse_ui_settings();
    void stop_all_processing();

    void create_frame();
    wxBoxSizer* create_out_format_radio_box(wxPanel* panel);
    wxBoxSizer* create_out_scrub_radio_box(wxPanel* panel);
    wxBoxSizer* create_out_settings_check_box(wxPanel* panel);
    wxBoxSizer* create_input_picker_box(wxPanel* panel);
    wxBoxSizer* create_output_picker_box(wxPanel* panel);
    wxBoxSizer* create_info_box(wxPanel* panel);
    wxBoxSizer* create_process_buttons_box(wxPanel* panel);

    std::string get_file_type_string(FileType type);
    const char* auto_format_to_string(AutoFormat format);
    const char* file_type_to_string(FileType type);
    const char* scrub_type_to_string(ScrubType type);
    void log_output_settings(const OutputSettings& settings);

    wxDECLARE_EVENT_TABLE();
};

#endif // _MAIN_FRAME_H_