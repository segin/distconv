#include "mock_subprocess.h"
#include <sstream>
#include <fstream>

namespace transcoding_engine {
using namespace distconv::TranscodingEngine;

using namespace distconv::TranscodingEngine;

SubprocessResult MockSubprocess::run(const std::vector<std::string>& command,
                                    const std::string& working_directory,
                                    int timeout_seconds) {
    record_call(command, "", working_directory, timeout_seconds);

    // Simulate side effects for ffmpeg: create output file
    if (!command.empty() && command[0] == "ffmpeg") {
        // Assume last argument is output file
        if (command.size() > 1) {
            std::string output_file = command.back();
            // Simple check: output file usually ends with .mp4 or similar, and is not a flag
            if (output_file.length() > 4 && output_file[0] != '-') {
                 std::ofstream file(output_file);
                 file << "mock output content";
                 file.close();
            }
        }
    }

    return get_result_for_command(command);
}

SubprocessResult MockSubprocess::run_with_input(const std::vector<std::string>& command,
                                               const std::string& stdin_input,
                                               const std::string& working_directory,
                                               int timeout_seconds) {
    record_call(command, stdin_input, working_directory, timeout_seconds);
    return get_result_for_command(command);
}

bool MockSubprocess::is_executable_available(const std::string& executable) {
    if (executable_availability_.count(executable)) {
        return executable_availability_[executable];
    }
    return true; // Default to available
}

std::string MockSubprocess::find_executable_path(const std::string& executable) {
    if (executable_paths_.count(executable)) {
        return executable_paths_[executable];
    }
    return "/usr/bin/" + executable; // Default mock path
}

void MockSubprocess::set_result_for_command(const std::vector<std::string>& command, const SubprocessResult& result) {
    command_results_[command] = result;
}

void MockSubprocess::set_default_result(const SubprocessResult& result) {
    default_result_ = result;
}

void MockSubprocess::set_executable_available(const std::string& executable, bool available) {
    executable_availability_[executable] = available;
}

void MockSubprocess::set_executable_path(const std::string& executable, const std::string& path) {
    executable_paths_[executable] = path;
}

void MockSubprocess::add_result_queue(const std::vector<std::string>& command, std::queue<SubprocessResult> results) {
    command_result_queues_[command] = results;
}

bool MockSubprocess::was_command_called(const std::vector<std::string>& command) const {
    for (const auto& call : call_history_) {
        if (call.command == command) {
            return true;
        }
    }
    return false;
}

bool MockSubprocess::was_executable_called(const std::string& executable) const {
    for (const auto& call : call_history_) {
        if (!call.command.empty() && call.command[0] == executable) {
            return true;
        }
    }
    return false;
}

MockSubprocess::CallInfo MockSubprocess::get_last_call() const {
    if (call_history_.empty()) {
        return {};
    }
    return call_history_.back();
}

SubprocessResult MockSubprocess::get_result_for_command(const std::vector<std::string>& command) {
    // Check queues first
    auto queue_it = command_result_queues_.find(command);
    if (queue_it != command_result_queues_.end() && !queue_it->second.empty()) {
        auto result = queue_it->second.front();
        queue_it->second.pop();
        return result;
    }
    
    // Check specific results
    auto it = command_results_.find(command);
    if (it != command_results_.end()) {
        return it->second;
    }
    
    // Return default result
    return default_result_;
}

void MockSubprocess::record_call(const std::vector<std::string>& command, const std::string& stdin_input,
                                const std::string& working_directory, int timeout_seconds) {
    call_history_.push_back({command, stdin_input, working_directory, timeout_seconds});
}

std::string MockSubprocess::command_to_string(const std::vector<std::string>& command) const {
    std::stringstream ss;
    for (size_t i = 0; i < command.size(); ++i) {
        if (i > 0) ss << " ";
        ss << command[i];
    }
    return ss.str();
}

} // namespace TranscodingEngine
} // namespace distconv
