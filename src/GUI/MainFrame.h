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

struct TypeStringPair
{
    FileType type;
    std::string string;
};

static const std::vector<TypeStringPair> TYPE_STR_MAP = 
{
    { FileType::ISO, "ISO" },
    { FileType::GoD, "GoD" },
    { FileType::CCI, "CCI" },
    { FileType::CSO, "CSO" },
    { FileType::ZAR, "ZAR" },
    { FileType::DIR, "DIR" },
    { FileType::XBE, "XBE" },
};

wxDECLARE_EVENT(wxEVT_UPDATE_CURRENT_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_UPDATE_TOTAL_PROGRESS, wxThreadEvent);
wxDECLARE_EVENT(wxEVT_THREAD_COMPLETED, wxThreadEvent);

class MainFrame : public wxFrame
{
public:
    MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    ~MainFrame();

    static void update_progress_bar(uint64_t current, uint64_t total);

private:
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

    std::unique_ptr<std::thread> processing_thread_{nullptr};
    std::atomic<bool> cancel_processing_{false};
    std::atomic<bool> paused_processing_{false};
    std::atomic<bool> currently_processing_{false};

    std::unique_ptr<InputHelper> input_helper_{nullptr};
    std::vector<std::filesystem::path> input_paths_;
    std::filesystem::path output_path_;

    Picker input_picker_;
    Picker output_picker_;

    wxListCtrl* file_list_{nullptr};

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
    void on_close(wxCloseEvent& event);
    void on_thread_completed(wxThreadEvent& event);

    void update_current_progress_bar(uint64_t progress, uint64_t total);
    void process_files();
    void terminate_processing_thread();
    void update_controls_state();
    void set_button_states(bool processing);
    OutputSettings parse_ui_settings();

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