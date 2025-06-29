#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include "cpr/cpr.h"
#include "nlohmann/json.hpp"

// Dispatch Server URL and API Key
const std::string DISPATCH_SERVER_URL = "http://localhost:8080";
const std::string API_KEY = "your-super-secret-api-key"; // Replace with your actual API key

// Local storage for job IDs
const std::string JOB_IDS_FILE = "submitted_job_ids.txt";

void saveJobId(const std::string& job_id) {
    std::ofstream ofs(JOB_IDS_FILE, std::ios_base::app);
    if (ofs.is_open()) {
        ofs << job_id << std::endl;
        ofs.close();
    }
}

std::vector<std::string> loadJobIds() {
    std::vector<std::string> job_ids;
    std::ifstream ifs(JOB_IDS_FILE);
    if (ifs.is_open()) {
        std::string line;
        while (std::getline(ifs, line)) {
            job_ids.push_back(line);
        }
        ifs.close();
    }
    return job_ids;
}

void submitJob(const std::string& source_url, const std::string& target_codec, double job_size, int max_retries) {
    nlohmann::json payload;
    payload["source_url"] = source_url;
    payload["target_codec"] = target_codec;
    payload["job_size"] = job_size;
    payload["max_retries"] = max_retries;

    cpr::Response r = cpr::Post(cpr::Url{DISPATCH_SERVER_URL + "/jobs/"},
                                 cpr::Header{{"X-API-Key", API_KEY}},
                                 cpr::Header{{"Content-Type", "application/json"}},
                                 cpr::Body{payload.dump()});

    if (r.status_code == 200) {
        nlohmann::json response_json = nlohmann::json::parse(r.text);
        std::cout << "Job submitted successfully:" << std::endl;
        std::cout << response_json.dump(4) << std::endl;
        saveJobId(response_json["job_id"]);
    } else {
        std::cerr << "Error submitting job: " << r.status_code << " - " << r.text << std::endl;
    }
}

void getJobStatus(const std::string& job_id) {
    cpr::Response r = cpr::Get(cpr::Url{DISPATCH_SERVER_URL + "/jobs/" + job_id},
                               cpr::Header{{"X-API-Key", API_KEY}});

    if (r.status_code == 200) {
        nlohmann::json response_json = nlohmann::json::parse(r.text);
        std::cout << "Status for job " << job_id << ":" << std::endl;
        std::cout << response_json.dump(4) << std::endl;
    } else {
        std::cerr << "Error getting job status: " << r.status_code << " - " << r.text << std::endl;
    }
}

void listAllJobs() {
    cpr::Response r = cpr::Get(cpr::Url{DISPATCH_SERVER_URL + "/jobs/"},
                               cpr::Header{{"X-API-Key", API_KEY}});

    if (r.status_code == 200) {
        nlohmann::json response_json = nlohmann::json::parse(r.text);
        std::cout << "All Jobs Status:" << std::endl;
        std::cout << response_json.dump(4) << std::endl;
    } else {
        std::cerr << "Error listing all jobs: " << r.status_code << " - " << r.text << std::endl;
    }
}

void listAllEngines() {
    cpr::Response r = cpr::Get(cpr::Url{DISPATCH_SERVER_URL + "/engines/"},
                               cpr::Header{{"X-API-Key", API_KEY}});

    if (r.status_code == 200) {
        nlohmann::json response_json = nlohmann::json::parse(r.text);
        std::cout << "All Engines Status:" << std::endl;
        std::cout << response_json.dump(4) << std::endl;
    } else {
        std::cerr << "Error listing all engines: " << r.status_code << " - " << r.text << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  submit <source_url> <target_codec> <job_size> [max_retries]" << std::endl;
        std::cout << "  status <job_id>" << std::endl;
        std::cout << "  list_jobs" << std::endl;
        std::cout << "  list_engines" << std::endl;
        std::cout << "  retrieve_local_jobs" << std::endl;
        return 1;
    }

    std::string command = argv[1];

    if (command == "submit") {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " submit <source_url> <target_codec> <job_size> [max_retries]" << std::endl;
            return 1;
        }
        std::string source_url = argv[2];
        std::string target_codec = argv[3];
        double job_size = std::stod(argv[4]);
        int max_retries = (argc > 5) ? std::stoi(argv[5]) : 3;
        submitJob(source_url, target_codec, job_size, max_retries);
    } else if (command == "status") {
        if (argc < 3) {
            std::cerr << "Usage: " << argv[0] << " status <job_id>" << std::endl;
            return 1;
        }
        getJobStatus(argv[2]);
    } else if (command == "list_jobs") {
        listAllJobs();
    } else if (command == "list_engines") {
        listAllEngines();
    } else if (command == "retrieve_local_jobs") {
        std::vector<std::string> job_ids = loadJobIds();
        if (job_ids.empty()) {
            std::cout << "No locally submitted job IDs found." << std::endl;
        } else {
            std::cout << "Locally submitted job IDs:" << std::endl;
            for (const std::string& id : job_ids) {
                std::cout << "- " << id << std::endl;
                getJobStatus(id); // Also fetch status for each locally stored job
            }
        }
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        return 1;
    }

    return 0;
}
