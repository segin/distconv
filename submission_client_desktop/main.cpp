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

int main() {
    std::string command;
    std::string input;

    while (true) {
        std::cout << "\nSelect an option:" << std::endl;
        std::cout << "1. Submit a new transcoding job\n";
        std::cout << "2. Get status of a job\n";
        std::cout << "3. List all jobs\n";
        std::cout << "4. List all engines\n";
        std::cout << "5. Retrieve locally submitted jobs\n";
        std::cout << "6. Exit\n";
        std::cout << "> ";
        std::getline(std::cin, input);

        try {
            int choice = std::stoi(input);
            switch (choice) {
                case 1: {
                    std::string source_url, target_codec;
                    double job_size;
                    int max_retries;

                    std::cout << "Enter source URL: ";
                    std::getline(std::cin, source_url);
                    std::cout << "Enter target codec (e.g., h264, h265): ";
                    std::getline(std::cin, target_codec);
                    std::cout << "Enter job size (e.g., 100.5): ";
                    std::getline(std::cin, input);
                    job_size = std::stod(input);
                    std::cout << "Enter max retries (default 3): ";
                    std::getline(std::cin, input);
                    max_retries = input.empty() ? 3 : std::stoi(input);

                    submitJob(source_url, target_codec, job_size, max_retries);
                    break;
                }
                case 2: {
                    std::string job_id;
                    std::cout << "Enter job ID: ";
                    std::getline(std::cin, job_id);
                    getJobStatus(job_id);
                    break;
                }
                case 3:
                    listAllJobs();
                    break;
                case 4:
                    listAllEngines();
                    break;
                case 5: {
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
                    break;
                }
                case 6:
                    std::cout << "Exiting..." << std::endl;
                    return 0;
                default:
                    std::cout << "Invalid choice. Please try again." << std::endl;
                    break;
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid input. Please enter a number for your choice or a valid numeric value for job size/retries." << std::endl;
        } catch (const std::out_of_range& e) {
            std::cerr << "Input out of range." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "An unexpected error occurred: " << e.what() << std::endl;
        }
    }

    return 0;
}
