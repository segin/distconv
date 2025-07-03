#include "submission_client_core.h"
#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include "cpr/cpr.h"
#include "nlohmann/json.hpp"

// Dispatch Server URL and API Key
// Global configuration (will be set via UI/command line)
std::string g_dispatchServerUrl = "http://localhost:8080";
std::string g_apiKey = "your-super-secret-api-key";
std::string g_caCertPath = "server.crt";

// Local storage for job IDs
std::string JOB_IDS_FILE = "submitted_job_ids.txt";

ApiClient::ApiClient(const std::string& server_url, const std::string& api_key, const std::string& ca_cert_path, std::unique_ptr<CprApi> cpr_api)
    : server_url_(server_url), api_key_(api_key), ca_cert_path_(ca_cert_path), cpr_api_(std::move(cpr_api)) {}

nlohmann::json ApiClient::submitJob(const std::string& source_url, const std::string& target_codec, double job_size, int max_retries) {
    nlohmann::json payload;
    payload["source_url"] = source_url;
    payload["target_codec"] = target_codec;
    payload["job_size"] = job_size;
    payload["max_retries"] = max_retries;

    cpr::SslOptions ssl_opts;
    if (!ca_cert_path_.empty()) {
        ssl_opts.ca_info = ca_cert_path_;
    } else {
        ssl_opts.verify_peer = false;
        ssl_opts.verify_host = false;
    }

    cpr::Response r = cpr_api_->Post(cpr::Url{server_url_ + "/jobs/"},
                                 cpr::Header{{"X-API-Key", api_key_}, {"Content-Type", "application/json"}},
                                 cpr::Body{payload.dump()},
                                 ssl_opts);

    if (r.status_code == 200) {
        return nlohmann::json::parse(r.text);
    } else {
        throw std::runtime_error("Error submitting job: " + std::to_string(r.status_code) + " - " + r.text);
    }
}

nlohmann::json ApiClient::getJobStatus(const std::string& job_id) {
    cpr::SslOptions ssl_opts;
    if (!ca_cert_path_.empty()) {
        ssl_opts.ca_info = ca_cert_path_;
    } else {
        ssl_opts.verify_peer = false;
        ssl_opts.verify_host = false;
    }

    cpr::Response r = cpr::Get(cpr::Url{server_url_ + "/jobs/" + job_id},
                               cpr::Header{{"X-API-Key", api_key_}},
                               ssl_opts);

    if (r.status_code == 200) {
        return nlohmann::json::parse(r.text);
    } else {
        throw std::runtime_error("Error getting job status: " + std::to_string(r.status_code) + " - " + r.text);
    }
}

nlohmann::json ApiClient::listAllJobs() {
    cpr::SslOptions ssl_opts;
    if (!ca_cert_path_.empty()) {
        ssl_opts.ca_info = ca_cert_path_;
    } else {
        ssl_opts.verify_peer = false;
        ssl_opts.verify_host = false;
    }

    cpr::Response r = cpr::Get(cpr::Url{server_url_ + "/jobs/"},
                               cpr::Header{{"X-API-Key", api_key_}},
                               ssl_opts);

    if (r.status_code == 200) {
        return nlohmann::json::parse(r.text);
    } else {
        throw std::runtime_error("Error listing jobs: " + std::to_string(r.status_code) + " - " + r.text);
    }
}

nlohmann::json ApiClient::listAllEngines() {
    cpr::SslOptions ssl_opts;
    if (!ca_cert_path_.empty()) {
        ssl_opts.ca_info = ca_cert_path_;
    } else {
        ssl_opts.verify_peer = false;
        ssl_opts.verify_host = false;
    }

    cpr::Response r = cpr::Get(cpr::Url{server_url_ + "/engines/"},
                               cpr::Header{{"X-API-Key", api_key_}},
                               ssl_opts);

    if (r.status_code == 200) {
        return nlohmann::json::parse(r.text);
    } else {
        throw std::runtime_error("Error listing engines: " + std::to_string(r.status_code) + " - " + r.text);
    }
}

// Function to save a job ID to a local file
void saveJobId(const std::string& job_id) {
    std::ofstream ofs(JOB_IDS_FILE, std::ios_base::app);
    if (ofs.is_open()) {
        ofs << job_id << std::endl;
        ofs.close();
    } else {
        std::cerr << "Error: Could not open " << JOB_IDS_FILE << " for writing." << std::endl;
    }
}

// Function to load job IDs from a local file
std::vector<std::string> loadJobIds() {
    std::vector<std::string> job_ids;
    std::ifstream ifs(JOB_IDS_FILE);
    if (ifs.is_open()) {
        std::string job_id;
        while (std::getline(ifs, job_id)) {
            job_ids.push_back(job_id);
        }
        ifs.close();
    }
    return job_ids;
}

// Define the main application class
class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

// Define the main frame class
class MyFrame : public wxFrame
{
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);

private:
    // Event handlers
    void OnSubmit(wxCommandEvent& event);
    void OnGetStatus(wxCommandEvent& event);
    void OnListJobs(wxCommandEvent& event);
    void OnListEngines(wxCommandEvent& event);
    void OnRetrieveLocalJobs(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);

    // UI elements
    wxTextCtrl *m_sourceUrlText;
    wxTextCtrl *m_targetCodecText;
    wxTextCtrl *m_jobSizeText;
    wxTextCtrl *m_maxRetriesText;
    wxTextCtrl *m_jobIdText;
    wxTextCtrl *m_outputLog;

    ApiClient apiClient_;
};

// Implement MyApp methods
bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame("Transcoding Submission Client", wxPoint(50, 50), wxSize(800, 600));
    frame->Show(true);
    return true;
}

// Implement MyFrame methods
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
    : wxFrame(NULL, wxID_ANY, title, pos, size),
      apiClient_(g_dispatchServerUrl, g_apiKey, g_caCertPath)
{
    wxPanel *panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

    // Job Submission Section
    wxStaticBoxSizer *submitSizer = new wxStaticBoxSizer(wxVERTICAL, panel, "Submit New Job");
    wxGridSizer *submitGridSizer = new wxGridSizer(2, 5, 5);

    submitGridSizer->Add(new wxStaticText(panel, wxID_ANY, "Source URL:"), 0, wxALIGN_CENTER_VERTICAL);
    m_sourceUrlText = new wxTextCtrl(panel, wxID_ANY);
    submitGridSizer->Add(m_sourceUrlText, 1, wxEXPAND);

    submitGridSizer->Add(new wxStaticText(panel, wxID_ANY, "Target Codec:"), 0, wxALIGN_CENTER_VERTICAL);
    m_targetCodecText = new wxTextCtrl(panel, wxID_ANY);
    submitGridSizer->Add(m_targetCodecText, 1, wxEXPAND);

    submitGridSizer->Add(new wxStaticText(panel, wxID_ANY, "Job Size:"), 0, wxALIGN_CENTER_VERTICAL);
    m_jobSizeText = new wxTextCtrl(panel, wxID_ANY);
    submitGridSizer->Add(m_jobSizeText, 1, wxEXPAND);

    submitGridSizer->Add(new wxStaticText(panel, wxID_ANY, "Max Retries:"), 0, wxALIGN_CENTER_VERTICAL);
    m_maxRetriesText = new wxTextCtrl(panel, wxID_ANY, "3");
    submitGridSizer->Add(m_maxRetriesText, 1, wxEXPAND);

    wxButton *submitButton = new wxButton(panel, wxID_ANY, "Submit Job");
    submitButton->Bind(wxEVT_BUTTON, &MyFrame::OnSubmit, this);
    submitSizer->Add(submitGridSizer, 1, wxEXPAND | wxALL, 5);
    submitSizer->Add(submitButton, 0, wxALIGN_CENTER | wxALL, 5);
    mainSizer->Add(submitSizer, 0, wxEXPAND | wxALL, 10);

    // Job Status Section
    wxStaticBoxSizer *statusSizer = new wxStaticBoxSizer(wxVERTICAL, panel, "Job Status");
    wxBoxSizer *statusInputSizer = new wxBoxSizer(wxHORIZONTAL);
    statusInputSizer->Add(new wxStaticText(panel, wxID_ANY, "Job ID:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    m_jobIdText = new wxTextCtrl(panel, wxID_ANY, "");
    statusInputSizer->Add(m_jobIdText, 1, wxEXPAND);
    wxButton *getStatusButton = new wxButton(panel, wxID_ANY, "Get Status");
    getStatusButton->Bind(wxEVT_BUTTON, &MyFrame::OnGetStatus, this);
    statusInputSizer->Add(getStatusButton, 0, wxLEFT, 5);
    statusSizer->Add(statusInputSizer, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(statusSizer, 0, wxEXPAND | wxALL, 10);

    // Action Buttons
    wxBoxSizer *actionButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton *listJobsButton = new wxButton(panel, wxID_ANY, "List All Jobs");
    listJobsButton->Bind(wxEVT_BUTTON, &MyFrame::OnListJobs, this);
    actionButtonSizer->Add(listJobsButton, 1, wxEXPAND | wxALL, 5);

    wxButton *listEnginesButton = new wxButton(panel, wxID_ANY, "List All Engines");
    listEnginesButton->Bind(wxEVT_BUTTON, &MyFrame::OnListEngines, this);
    actionButtonSizer->Add(listEnginesButton, 1, wxEXPAND | wxALL, 5);

    wxButton *retrieveLocalJobsButton = new wxButton(panel, wxID_ANY, "Retrieve Local Jobs");
    retrieveLocalJobsButton->Bind(wxEVT_BUTTON, &MyFrame::OnRetrieveLocalJobs, this);
    actionButtonSizer->Add(retrieveLocalJobsButton, 1, wxEXPAND | wxALL, 5);

    mainSizer->Add(actionButtonSizer, 0, wxEXPAND | wxALL, 10);

    // Output Log
    m_outputLog = new wxTextCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
    mainSizer->Add(m_outputLog, 1, wxEXPAND | wxALL, 10);

    panel->SetSizerAndFit(mainSizer);
    this->Centre();
}

// Event handler implementations
void MyFrame::OnSubmit(wxCommandEvent& event)
{
    std::string source_url = m_sourceUrlText->GetValue().ToStdString();
    std::string target_codec = m_targetCodecText->GetValue().ToStdString();
    std::string job_size_str = m_jobSizeText->GetValue().ToStdString();
    std::string max_retries_str = m_maxRetriesText->GetValue().ToStdString();

    if (source_url.empty() || target_codec.empty() || job_size_str.empty()) {
        m_outputLog->AppendText("Error: Source URL, Target Codec, and Job Size cannot be empty.\n");
        return;
    }

    try {
        double job_size = std::stod(job_size_str);
        int max_retries = max_retries_str.empty() ? 3 : std::stoi(max_retries_str);

        nlohmann::json response = apiClient_.submitJob(source_url, target_codec, job_size, max_retries);
        m_outputLog->AppendText("Job submitted successfully:\n");
        m_outputLog->AppendText(response.dump(4) + "\n");
        saveJobId(response["job_id"]);

    } catch (const std::exception& e) {
        m_outputLog->AppendText(wxString::Format("An error occurred: %s\n", e.what()));
    }
}

void MyFrame::OnGetStatus(wxCommandEvent& event)
{
    std::string job_id = m_jobIdText->GetValue().ToStdString();
    if (job_id.empty()) {
        m_outputLog->AppendText("Error: Job ID cannot be empty.\n");
        return;
    }

    try {
        nlohmann::json response = apiClient_.getJobStatus(job_id);
        m_outputLog->AppendText("Job Status for " + job_id + ":\n");
        m_outputLog->AppendText(response.dump(4) + "\n");
    } catch (const std::exception& e) {
        m_outputLog->AppendText(wxString::Format("An error occurred: %s\n", e.what()));
    }
}

void MyFrame::OnListJobs(wxCommandEvent& event)
{
    try {
        nlohmann::json response = apiClient_.listAllJobs();
        m_outputLog->AppendText("All Jobs:\n");
        m_outputLog->AppendText(response.dump(4) + "\n");
    } catch (const std::exception& e) {
        m_outputLog->AppendText(wxString::Format("An error occurred: %s\n", e.what()));
    }
}

void MyFrame::OnListEngines(wxCommandEvent& event)
{
    try {
        nlohmann::json response = apiClient_.listAllEngines();
        m_outputLog->AppendText("All Engines:\n");
        m_outputLog->AppendText(response.dump(4) + "\n");
    } catch (const std::exception& e) {
        m_outputLog->AppendText(wxString::Format("An error occurred: %s\n", e.what()));
    }
}

void MyFrame::OnRetrieveLocalJobs(wxCommandEvent& event)
{
    std::vector<std::string> job_ids = loadJobIds();
    if (job_ids.empty()) {
        m_outputLog->AppendText("No locally submitted job IDs found.\n");
    }
    else {
        m_outputLog->AppendText("Locally submitted job IDs:\n");
        for (const std::string& id : job_ids) {
            m_outputLog->AppendText("- " + id + "\n");
            try {
                nlohmann::json response = apiClient_.getJobStatus(id);
                m_outputLog->AppendText(response.dump(4) + "\n");
            } catch (const std::exception& e) {
                m_outputLog->AppendText(wxString::Format("An error occurred while getting status for job %s: %s\n", id, e.what()));
            }
        }
    }
}

void MyFrame::OnExit(wxCommandEvent& event)
{
    Close(true);
}

// Implement the wxWidgets application entry point
int run_submission_client(int argc, char* argv[]) {
    wxApp::SetInstance(new MyApp());
    return wxEntry(argc, argv);
}
