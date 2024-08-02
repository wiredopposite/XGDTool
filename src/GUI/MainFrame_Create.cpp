#include "GUI/MainFrame.h"

void MainFrame::create_frame()
{
    #ifdef WIN32
        wxIcon icon;
        icon.LoadFile("IDI_APP_ICON", wxBITMAP_TYPE_ICO_RESOURCE);
        SetIcon(icon);
    #endif

    wxPanel* panel = new wxPanel(this, wxID_ANY);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* fg_sizer = new wxFlexGridSizer(12, 2, 10, 10);
    fg_sizer->AddGrowableCol(1, 1); 
    fg_sizer->AddGrowableRow(2, 1);

    wxStaticText* input_label = new wxStaticText(panel, wxID_ANY, "Input Path:");
    fg_sizer->Add(input_label, 0, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    fg_sizer->Add(create_input_picker_box(panel), 1, wxEXPAND);

    wxStaticText* output_label = new wxStaticText(panel, wxID_ANY, "Output Directory:");
    fg_sizer->Add(output_label, 1, wxALIGN_CENTER_VERTICAL | wxALIGN_RIGHT);
    fg_sizer->Add(create_output_picker_box(panel), 1, wxEXPAND);

    wxStaticText* file_list_label = new wxStaticText(panel, wxID_ANY, "File List:");
    fg_sizer->Add(file_list_label, 2, wxALIGN_TOP | wxALIGN_RIGHT);

    file_list_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    file_list_->InsertColumn(0, "Format", wxLIST_FORMAT_LEFT, 60);
    file_list_->InsertColumn(1, "Filename", wxLIST_FORMAT_LEFT, 600);

    fg_sizer->Add(file_list_, 1, wxEXPAND);

    wxBoxSizer* progress_lables_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* current_progress_label = new wxStaticText(panel, wxID_ANY, "Current Progress:");
    wxStaticText* total_progress_label = new wxStaticText(panel, wxID_ANY, "Total Progress:");
    wxStaticText* status_label = new wxStaticText(panel, wxID_ANY, "Status:");

    progress_lables_sizer->Add(status_label, 0, wxALIGN_RIGHT);
    progress_lables_sizer->AddSpacer(18);
    progress_lables_sizer->Add(current_progress_label, 0, wxALIGN_RIGHT);
    progress_lables_sizer->AddSpacer(18);
    progress_lables_sizer->Add(total_progress_label, 0, wxALIGN_RIGHT);
    progress_lables_sizer->AddSpacer(4);

    fg_sizer->Add(progress_lables_sizer, 0, wxALIGN_BOTTOM);

    wxBoxSizer* bottom_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* settings_progress_bar_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* settings_sizer = new wxBoxSizer(wxHORIZONTAL);

    settings_sizer->Add(create_out_format_radio_box(panel), 0, wxEXPAND);
    settings_sizer->AddSpacer(80);
    settings_sizer->Add(create_out_scrub_radio_box(panel), 0, wxEXPAND);
    settings_sizer->AddSpacer(80);
    settings_sizer->Add(create_out_settings_check_box(panel), 0, wxEXPAND);

    settings_progress_bar_sizer->Add(settings_sizer, 0, wxEXPAND);
    settings_progress_bar_sizer->AddSpacer(10);

    current_progress_bar_ = new wxGauge(panel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 25));
    total_progress_bar_ = new wxGauge(panel, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 25));

    status_field_ = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    status_field_->SetBackgroundStyle(wxBG_STYLE_ERASE);
    status_field_->SetLabel("Idle");

    settings_progress_bar_sizer->Add(status_field_, 0, wxEXPAND);
    settings_progress_bar_sizer->AddSpacer(10);
    settings_progress_bar_sizer->Add(current_progress_bar_, 0, wxEXPAND);
    settings_progress_bar_sizer->AddSpacer(10);
    settings_progress_bar_sizer->Add(total_progress_bar_, 0, wxEXPAND);

    bottom_sizer->Add(settings_progress_bar_sizer, 1, wxEXPAND);
    bottom_sizer->AddSpacer(10);

    bottom_sizer->Add(create_process_buttons_box(panel), 0, wxALIGN_BOTTOM);

    fg_sizer->Add(bottom_sizer, 10, wxEXPAND);
    fg_sizer->AddSpacer(10);
    fg_sizer->Add(create_info_box(panel), 11, wxALIGN_CENTER_VERTICAL | wxALIGN_LEFT);

    main_sizer->Add(fg_sizer, 1, wxALL | wxEXPAND, 10);

    panel->SetSizer(main_sizer);

    update_controls_state();
}

wxBoxSizer* MainFrame::create_input_picker_box(wxPanel* panel)
{
    wxBoxSizer* input_sizer = new wxBoxSizer(wxHORIZONTAL);
    input_picker_.field = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    input_picker_.field->SetBackgroundColour(wxColour(250, 250, 250));

    input_sizer->Add(input_picker_.field, 1, wxEXPAND);

    input_picker_.button = new wxButton(panel, wxID_ANY, "Browse");
    input_picker_.button->SetToolTip("Select the input file or directory to process");
    input_picker_.button->Bind(wxEVT_BUTTON, &MainFrame::on_pick_input_path, this);

    input_sizer->Add(input_picker_.button, 0, wxLEFT, 5);

    return input_sizer;
}

wxBoxSizer* MainFrame::create_output_picker_box(wxPanel* panel)
{
    wxBoxSizer* output_sizer = new wxBoxSizer(wxHORIZONTAL);
    output_picker_.field = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    output_picker_.field->SetBackgroundColour(wxColour(250, 250, 250));

    output_sizer->Add(output_picker_.field, 1, wxEXPAND);

    output_picker_.button = new wxButton(panel, wxID_ANY, "Browse");
    output_picker_.button->SetToolTip("Select the output directory to save the processed files");
    output_picker_.button->Bind(wxEVT_BUTTON, &MainFrame::on_pick_output_path, this);

    output_sizer->Add(output_picker_.button, 0, wxLEFT, 5);

    return output_sizer;    
}

wxBoxSizer* MainFrame::create_process_buttons_box(wxPanel* panel)
{
    wxBoxSizer* buttons_sizer = new wxBoxSizer(wxVERTICAL);

    process_buttons_.process = new wxButton(panel, wxID_ANY, "Process All", wxDefaultPosition, wxSize(100, 25));
    process_buttons_.pause = new wxButton(panel, wxID_ANY, "Pause", wxDefaultPosition, wxSize(100, 25));
    process_buttons_.cancel = new wxButton(panel, wxID_ANY, "Cancel", wxDefaultPosition, wxSize(100, 25));

    process_buttons_.process->SetToolTip("Process all files in the File List");
    process_buttons_.process->Bind(wxEVT_BUTTON, &MainFrame::on_process_all, this);

    process_buttons_.pause->SetToolTip("Pause processing of files");
    process_buttons_.pause->Bind(wxEVT_BUTTON, &MainFrame::on_pause_process, this);

    process_buttons_.cancel->SetToolTip("Processing will stop after the current file is finished");
    process_buttons_.cancel->Bind(wxEVT_BUTTON, &MainFrame::on_cancel_process, this);

    buttons_sizer->Add(process_buttons_.process, 0, wxEXPAND);
    buttons_sizer->AddSpacer(10);
    buttons_sizer->Add(process_buttons_.pause, 0, wxEXPAND);
    buttons_sizer->AddSpacer(10);
    buttons_sizer->Add(process_buttons_.cancel, 0, wxEXPAND);

    return buttons_sizer;
}

wxBoxSizer* MainFrame::create_out_scrub_radio_box(wxPanel* panel)
{
    wxBoxSizer* scrub_rbs_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* scrub_label = new wxStaticText(panel, wxID_ANY, "Scrub:", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    scrub_rbs_sizer->Add(scrub_label, 0, wxALIGN_LEFT);
    scrub_rbs_sizer->AddSpacer(5);

    out_scrub_rbs_.none = new wxRadioButton(panel, wxID_ANY, "None", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    out_scrub_rbs_.partial = new wxRadioButton(panel, wxID_ANY, "Partial");
    out_scrub_rbs_.full = new wxRadioButton(panel, wxID_ANY, "Full");

    out_scrub_rbs_.none->SetToolTip("No scrubbing, only video partion is removed if present");
    out_scrub_rbs_.partial->SetToolTip("Scrubs and trims the output image, random padding data is removed");
    out_scrub_rbs_.full->SetToolTip("Completely reauthor the resulting image, this will produce the smallest file possible");

    scrub_rbs_sizer->Add(out_scrub_rbs_.none, 0, wxEXPAND);
    scrub_rbs_sizer->Add(out_scrub_rbs_.partial, 0, wxEXPAND);
    scrub_rbs_sizer->Add(out_scrub_rbs_.full, 0, wxEXPAND);

    return scrub_rbs_sizer;
}

wxBoxSizer* MainFrame::create_out_settings_check_box(wxPanel* panel)
{
    wxBoxSizer* out_settings_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* out_settings_label = new wxStaticText(panel, wxID_ANY, "Settings:", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);

    out_settings_sizer->Add(out_settings_label, 0, wxALIGN_LEFT);
    out_settings_sizer->AddSpacer(5);

    out_settings_cbs_.split = new wxCheckBox(panel, wxID_ANY, "Split XISO");
    out_settings_cbs_.attach_xbe = new wxCheckBox(panel, wxID_ANY, "Generate Attach XBE");
    out_settings_cbs_.allowed_media_xbe = new wxCheckBox(panel, wxID_ANY, "Allowed Media XBE Patch");
    out_settings_cbs_.rename_xbe = new wxCheckBox(panel, wxID_ANY, "Rename XBE Title");
    out_settings_cbs_.offline_mode = new wxCheckBox(panel, wxID_ANY, "Offline Mode");
    
    out_settings_cbs_.split->SetToolTip("Splits the resulting XISO file if it's too large for OG Xbox");
    out_settings_cbs_.attach_xbe->SetToolTip("Generates an attach XBE file along with the output file");
    out_settings_cbs_.allowed_media_xbe->SetToolTip("Patches the Allowed Media field in resulting XBE files");
    out_settings_cbs_.rename_xbe->SetToolTip("Replaces the title field of resulting XBE files with one found in the database");
    out_settings_cbs_.offline_mode->SetToolTip("Disables online functionality, will result in less accurate file naming");
    
    out_settings_sizer->Add(out_settings_cbs_.split, 0, wxEXPAND);
    out_settings_sizer->Add(out_settings_cbs_.attach_xbe, 0, wxEXPAND);
    out_settings_sizer->Add(out_settings_cbs_.allowed_media_xbe, 0, wxEXPAND);
    out_settings_sizer->Add(out_settings_cbs_.rename_xbe, 0, wxEXPAND);
    out_settings_sizer->Add(out_settings_cbs_.offline_mode, 0, wxEXPAND);

    return out_settings_sizer;
}

wxBoxSizer* MainFrame::create_out_format_radio_box(wxPanel* panel)
{
    wxBoxSizer* out_format_sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* out_format_label_1 = new wxStaticText(panel, wxID_ANY, "Output Format:", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);

    out_format_sizer->Add(out_format_label_1, 0, wxALIGN_LEFT);
    out_format_sizer->AddSpacer(5);

    wxBoxSizer* out_format_rbox = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* out_format_rbox_1 = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* out_format_rbox_2 = new wxBoxSizer(wxVERTICAL);

    out_format_rbs_.iso     = new wxRadioButton(panel, wxID_ANY, "XISO", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    out_format_rbs_.god     = new wxRadioButton(panel, wxID_ANY, "GoD");
    out_format_rbs_.cci     = new wxRadioButton(panel, wxID_ANY, "CCI");
    out_format_rbs_.cso     = new wxRadioButton(panel, wxID_ANY, "CSO");
    out_format_rbs_.zar     = new wxRadioButton(panel, wxID_ANY, "ZAR");
    out_format_rbs_.extract = new wxRadioButton(panel, wxID_ANY, "Extract");

    auto_format_rbs_.ogxbox  = new wxRadioButton(panel, wxID_ANY, "OG XBox");
    auto_format_rbs_.xbox360 = new wxRadioButton(panel, wxID_ANY, "Xbox 360");
    auto_format_rbs_.xemu    = new wxRadioButton(panel, wxID_ANY, "Xemu");
    auto_format_rbs_.xenia   = new wxRadioButton(panel, wxID_ANY, "Xenia");

    out_format_rbs_.iso->SetToolTip("Creates an XISO image");
    out_format_rbs_.god->SetToolTip("Creates a Games on Demand image");
    out_format_rbs_.cci->SetToolTip("Creates a CCI archive");
    out_format_rbs_.cso->SetToolTip("Creates a CSO archive");
    out_format_rbs_.zar->SetToolTip("Creates a ZAR archive");
    
    out_format_rbs_.extract->SetToolTip("Extracts all files to a directory");
    auto_format_rbs_.ogxbox->SetToolTip("Automatically choose format and settings for use with OG Xbox");
    auto_format_rbs_.xbox360->SetToolTip("Automatically choose format and settings for use with Xbox 360");
    auto_format_rbs_.xemu->SetToolTip("Automatically choose format and settings for use with Xemu");
    auto_format_rbs_.xenia->SetToolTip("Automatically choose format and settings for use with Xenia");

    out_format_rbox_1->Add(out_format_rbs_.iso, 0, wxEXPAND);
    out_format_rbox_1->Add(out_format_rbs_.god, 0, wxEXPAND);
    out_format_rbox_1->Add(out_format_rbs_.cci, 0, wxEXPAND);
    out_format_rbox_1->Add(out_format_rbs_.cso, 0, wxEXPAND);
    out_format_rbox_1->Add(out_format_rbs_.zar, 0, wxEXPAND);
    out_format_rbox_1->Add(out_format_rbs_.extract, 0, wxEXPAND);

    out_format_rbox_2->Add(auto_format_rbs_.ogxbox, 0, wxEXPAND);
    out_format_rbox_2->Add(auto_format_rbs_.xbox360, 0, wxEXPAND);
    out_format_rbox_2->Add(auto_format_rbs_.xemu, 0, wxEXPAND);
    out_format_rbox_2->Add(auto_format_rbs_.xenia, 0, wxEXPAND);
    
    out_format_rbox->Add(out_format_rbox_1, 0, wxEXPAND);
    out_format_rbox->AddSpacer(10);
    out_format_rbox->Add(out_format_rbox_2, 0, wxEXPAND);
    out_format_sizer->Add(out_format_rbox, 0, wxEXPAND);

    out_format_rbs_.extract->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    out_format_rbs_.iso->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    out_format_rbs_.god->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    out_format_rbs_.cci->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    out_format_rbs_.cso->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    out_format_rbs_.zar->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });

    auto_format_rbs_.ogxbox->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    auto_format_rbs_.xbox360->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    auto_format_rbs_.xemu->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });
    auto_format_rbs_.xenia->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&) { update_controls_state(); });

    return out_format_sizer;
}

wxBoxSizer* MainFrame::create_info_box(wxPanel* panel)
{
    wxBoxSizer* wo_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* version_label = new wxStaticText(panel, wxID_ANY, wxString("v") + XGD::VERSION);
    wxStaticText* wo_label = new wxStaticText(panel, wxID_ANY, " | By WiredOpposite: ");
    wxHyperlinkCtrl* wo_link = new wxHyperlinkCtrl(panel, wxID_ANY, "wiredopposite.com", "https://wiredopposite.com");
    wxStaticText* wo_text = new wxStaticText(panel, wxID_ANY, " | Github: ");
    wxHyperlinkCtrl* wo_github_link = new wxHyperlinkCtrl(panel, wxID_ANY, "wiredopposite/xgdtool", "https://github.com/wiredopposite/xgdtool");
    
    wo_sizer->Add(version_label, 0, wxALIGN_CENTER_VERTICAL);
    wo_sizer->Add(wo_label, 0, wxALIGN_CENTER_VERTICAL);
    wo_sizer->Add(wo_link, 0, wxALIGN_CENTER_VERTICAL);
    wo_sizer->Add(wo_text, 0, wxALIGN_CENTER_VERTICAL);
    wo_sizer->Add(wo_github_link, 0, wxALIGN_CENTER_VERTICAL);

    return wo_sizer;
}