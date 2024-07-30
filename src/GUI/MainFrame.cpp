#include "GUI/MainFrame.h"

wxDEFINE_EVENT(wxEVT_UPDATE_CURRENT_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_UPDATE_TOTAL_PROGRESS, wxThreadEvent);
wxDEFINE_EVENT(wxEVT_THREAD_COMPLETED, wxThreadEvent);

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_CLOSE(MainFrame::on_close)
    EVT_BUTTON(wxID_ANY, MainFrame::on_process_all)
    EVT_BUTTON(wxID_ANY, MainFrame::on_cancel_process)
    EVT_THREAD(wxEVT_UPDATE_CURRENT_PROGRESS, MainFrame::on_update_current_progress)
    EVT_THREAD(wxEVT_UPDATE_TOTAL_PROGRESS, MainFrame::on_update_total_progress)
    EVT_THREAD(wxEVT_THREAD_COMPLETED, MainFrame::on_thread_completed)
wxEND_EVENT_TABLE()

wxGauge* MainFrame::current_progress_bar_ = nullptr;

MainFrame::MainFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(nullptr, wxID_ANY, title, pos, size) 
{
    create_frame();
    set_button_states(false);

    Bind(wxEVT_UPDATE_CURRENT_PROGRESS, &MainFrame::on_update_current_progress, this);
    Bind(wxEVT_UPDATE_TOTAL_PROGRESS, &MainFrame::on_update_total_progress, this);
    Bind(wxEVT_THREAD_COMPLETED, &MainFrame::on_thread_completed, this);
}

MainFrame::~MainFrame()
{
    terminate_processing_thread();
}

void MainFrame::on_close(wxCloseEvent& event)
{
    terminate_processing_thread();
    event.Skip();
}

void MainFrame::on_pause_process(wxCommandEvent& event)
{
    paused_processing_ = !paused_processing_;
    process_buttons_.pause->SetLabel(paused_processing_ ? "Resume" : "Pause");
    wxLogMessage(paused_processing_ ? "Processing will be paused after the current file is finished" : "Processing resumed");
}

void MainFrame::on_cancel_process(wxCommandEvent& event)
{
    cancel_processing_ = true;
    wxLogMessage("Processing will be cancelled after the current file is finished");
}

void MainFrame::terminate_processing_thread()
{
    if (processing_thread_ && processing_thread_->joinable())
    {
        cancel_processing_ = true;
        processing_thread_->join();
        processing_thread_.reset();
    }
    
    currently_processing_ = false;
}

void MainFrame::on_thread_completed(wxThreadEvent& event)
{
    terminate_processing_thread();

    if (input_helper_->failed_inputs().size() > 0)
    {
        for (const auto& failed_input : input_helper_->failed_inputs())
        {
            wxLogMessage("Failed to process input: " + wxString(failed_input.string()));
        }
    }
    else
    {
        wxLogMessage("Finished processing input files");
    }

    set_button_states(currently_processing_);
    update_current_progress_bar(100, 100);
    total_progress_bar_->SetRange(1);
    total_progress_bar_->SetValue(1);
}

void MainFrame::on_process_all(wxCommandEvent& event)
{
    if (input_paths_.empty())
    {
        wxLogMessage("No input files selected");
        return;
    }

    if (output_path_.empty())
    {
        wxLogMessage("No output directory selected");
        return;
    }

    input_helper_.reset();
    input_helper_ = std::make_unique<InputHelper>(input_paths_, output_path_, parse_ui_settings());

    if (input_helper_->input_infos().empty())
    {
        wxLogMessage("No valid files found in the selected input path");
        return;
    }

    total_progress_bar_->SetRange(input_helper_->input_infos().size());
    total_progress_bar_->SetValue(0);

    currently_processing_ = true;
    paused_processing_ = false;
    cancel_processing_ = false;

    set_button_states(currently_processing_);
    
    processing_thread_ = std::make_unique<std::thread>(&MainFrame::process_files, this);
}

void MainFrame::process_files()
{
    uint64_t total = input_helper_->input_infos().size();
    uint64_t progress = 0;

    for (const auto& input_info : input_helper_->input_infos())
    {
        input_helper_->process_single(input_info);
        progress++;

        wxThreadEvent* total_progress_event = new wxThreadEvent(wxEVT_UPDATE_TOTAL_PROGRESS);
        total_progress_event->SetPayload(std::make_pair(progress, total));
        wxQueueEvent(this, total_progress_event);

        while (paused_processing_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        if (cancel_processing_)
        {
            break;
        }
    }

    wxThreadEvent* thread_completed_event = new wxThreadEvent(wxEVT_THREAD_COMPLETED);
    wxQueueEvent(this, thread_completed_event);
}

void MainFrame::on_pick_input_path(wxCommandEvent& event)
{
    wxString choices[] = { "Select File(s)", "Select Directory" };
    int choice = wxGetSingleChoiceIndex("Choose the type of selection:", "Select", 2, choices, this);

    file_list_->DeleteAllItems();
    input_paths_.clear();
    input_helper_.reset();

    if (choice == 0) // Select File(s)
    {
        wxString wildcard = "Xbox image files (*.iso;*.cci;*.cso;*.zar;)|*.iso;*.cci;*.cso;*.zar;|All files (*.*)|*.*";
        wxFileDialog open_file_dialog(this, "Select file(s)", "", "", wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

        if (open_file_dialog.ShowModal() == wxID_OK)
        {
            wxArrayString file_paths;
            open_file_dialog.GetPaths(file_paths);

            for (const auto& file_path : file_paths)
            {
                input_picker_.field->AppendText(file_path + "\n"); 
                input_paths_.push_back(std::filesystem::path(file_path.ToStdString()));
            }
        }
    }
    else if (choice == 1) // Select Directory
    {
        wxDirDialog open_dir_dialog(this, "Select a directory", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

        if (open_dir_dialog.ShowModal() == wxID_OK)
        {
            wxString dir_path = open_dir_dialog.GetPath();
            input_picker_.field->SetValue(dir_path);
            input_paths_.push_back(dir_path.ToStdString());
        }
    }

    if (input_paths_.empty())
    {
        return;
    }

    input_helper_ = std::make_unique<InputHelper>(input_paths_, "", OutputSettings());

    if (input_helper_->input_infos().empty())
    {
        wxLogMessage("No valid files found in selected input path");
        return;
    }

    for (const auto& input_info : input_helper_->input_infos())
    {
        long item_index = file_list_->InsertItem(file_list_->GetItemCount(), get_file_type_string(input_info.file_type));
        file_list_->SetItem(item_index, 1, input_info.paths.front().filename().string());
    }
}

void MainFrame::on_pick_output_path(wxCommandEvent& event)
{
    wxDirDialog open_dir_dialog(this, "Select a GoD/Game/Batch directory", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

    if (open_dir_dialog.ShowModal() == wxID_OK)
    {
        wxString dir_path = open_dir_dialog.GetPath();
        output_picker_.field->SetValue(dir_path);
        output_path_ = std::filesystem::path(dir_path.ToStdString());
    }
}

void MainFrame::update_progress_bar(uint64_t progress, uint64_t total)
{
    wxThreadEvent* event = new wxThreadEvent(wxEVT_UPDATE_CURRENT_PROGRESS);
    event->SetPayload(std::make_pair(progress, total));
    wxQueueEvent(wxTheApp->GetTopWindow(), event);
}

void MainFrame::update_current_progress_bar(uint64_t progress, uint64_t total)
{
    if (!current_progress_bar_) 
    {
        return;
    }

    double percentage = static_cast<double>(progress) / static_cast<double>(total) * 100.0;
    auto gauge_range = current_progress_bar_->GetRange();
    int gauge_value = static_cast<int>(percentage / 100.0 * gauge_range);

    if (gauge_value < 0) 
    {
        gauge_value = 0;
    } 
    else if (gauge_value > gauge_range) 
    {
        gauge_value = gauge_range;
    }

    current_progress_bar_->SetValue(gauge_value);
}

void MainFrame::on_update_current_progress(wxThreadEvent& event)
{
    auto data = event.GetPayload<std::pair<uint64_t, uint64_t>>();
    uint64_t progress = data.first;
    uint64_t total = data.second;

    update_current_progress_bar(progress, total);
}

void MainFrame::on_update_total_progress(wxThreadEvent& event)
{
    auto data = event.GetPayload<std::pair<uint64_t, uint64_t>>();
    uint64_t progress = data.first;
    uint64_t total = data.second;

    if (total_progress_bar_) 
    {
        total_progress_bar_->SetRange(total);
        total_progress_bar_->SetValue(progress);
    }
}

std::string MainFrame::get_file_type_string(FileType type)
{
    for (const auto& pair : TYPE_STR_MAP)
    {
        if (pair.type == type)
        {
            return pair.string;
        }
    }

    return "UNKNOWN";
}

void MainFrame::update_controls_state()
{
    bool enable_scrub_options = out_format_rbs_.iso->GetValue() || out_format_rbs_.god->GetValue() || out_format_rbs_.cci->GetValue() || out_format_rbs_.cso->GetValue();
    out_scrub_rbs_.none->Enable(enable_scrub_options);
    out_scrub_rbs_.partial->Enable(enable_scrub_options);
    out_scrub_rbs_.full->Enable(enable_scrub_options);
    if (!enable_scrub_options)
    {
        out_scrub_rbs_.none->SetValue(true);
    }

    bool enable_split_option = out_format_rbs_.iso->GetValue();
    out_settings_cbs_.split->Enable(enable_split_option);
    if (!enable_split_option)
    {
        out_settings_cbs_.split->SetValue(false);
    }

    bool enable_attach_xbe_option = out_format_rbs_.iso->GetValue() || out_format_rbs_.cci->GetValue() || out_format_rbs_.cso->GetValue();
    out_settings_cbs_.attach_xbe->Enable(enable_attach_xbe_option);
    if (!enable_attach_xbe_option)
    {
        out_settings_cbs_.attach_xbe->SetValue(false);
    }

    bool allowed_media_patch = out_format_rbs_.extract->GetValue();
    out_settings_cbs_.allowed_media_xbe->Enable(allowed_media_patch);
    if (!allowed_media_patch)
    {
        out_settings_cbs_.allowed_media_xbe->SetValue(false);
    }

    bool enable_rename_xbe_option = out_format_rbs_.extract->GetValue();
    out_settings_cbs_.rename_xbe->Enable(enable_rename_xbe_option);
    if (!enable_rename_xbe_option)
    {
        out_settings_cbs_.rename_xbe->SetValue(false);
    }
}

void MainFrame::set_button_states(bool processing)
{
    process_buttons_.process->Enable(!processing);
    process_buttons_.pause->Enable(processing);
    process_buttons_.pause->SetLabel(!paused_processing_ ? "Pause" : "Resume");
    process_buttons_.cancel->Enable(processing);
    input_picker_.button->Enable(!processing);
    output_picker_.button->Enable(!processing);
}

OutputSettings MainFrame::parse_ui_settings()
{
    OutputSettings output_settings;

    if (out_format_rbs_.iso->GetValue())
    {
        output_settings.file_type = FileType::ISO;
    }
    else if (out_format_rbs_.god->GetValue())
    {
        output_settings.file_type = FileType::GoD;
    }
    else if (out_format_rbs_.cci->GetValue())
    {
        output_settings.file_type = FileType::CCI;
    }
    else if (out_format_rbs_.cso->GetValue())
    {
        output_settings.file_type = FileType::CSO;
    }
    else if (out_format_rbs_.zar->GetValue())
    {
        output_settings.file_type = FileType::ZAR;
    }
    else if (out_format_rbs_.extract->GetValue())
    {
        output_settings.file_type = FileType::DIR;
    }
    else if (auto_format_rbs_.ogxbox->GetValue())
    {
        output_settings.auto_format = AutoFormat::OGXBOX;
    }
    else if (auto_format_rbs_.xbox360->GetValue())
    {
        output_settings.auto_format = AutoFormat::XBOX360;
    }
    else if (auto_format_rbs_.xemu->GetValue())
    {
        output_settings.auto_format = AutoFormat::XEMU;
    }
    else if (auto_format_rbs_.xenia->GetValue())
    {
        output_settings.auto_format = AutoFormat::XENIA;
    }

    if (out_scrub_rbs_.none->GetValue())
    {
        output_settings.scrub_type = ScrubType::NONE;
    }
    else if (out_scrub_rbs_.partial->GetValue())
    {
        output_settings.scrub_type = ScrubType::PARTIAL;
    }
    else if (out_scrub_rbs_.full->GetValue())
    {
        output_settings.scrub_type = ScrubType::FULL;
    }

    output_settings.split = out_settings_cbs_.split->GetValue();
    output_settings.attach_xbe = out_settings_cbs_.attach_xbe->GetValue();
    output_settings.allowed_media_patch = out_settings_cbs_.allowed_media_xbe->GetValue();
    output_settings.rename_xbe = out_settings_cbs_.rename_xbe->GetValue();
    output_settings.offline_mode = out_settings_cbs_.offline_mode->GetValue();

    return output_settings;
}