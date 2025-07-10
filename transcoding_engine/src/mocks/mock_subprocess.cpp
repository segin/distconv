#include "mock_subprocess.h"
#include <sstream>
#include <algorithm>

namespace transcoding_engine {

SubprocessResult MockSubprocess::run(const std::vector<std::string>& command,
                                    const std::string& working_directory,
                                    int timeout_seconds) {
    record_call(command, "", working_directory, timeout_seconds);
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
    auto it = executable_availability_.find(executable);
    if (it != executable_availability_.end()) {
        return it->second;
    }
    // Default: assume common executables are available
    return (executable == "ffmpeg" || executable == "echo" || executable == "cat");
}

std::string MockSubprocess::find_executable_path(const std::string& executable) {
    auto it = executable_paths_.find(executable);
    if (it != executable_paths_.end()) {
        return it->second;
    }
    
    // Default paths for common executables
    if (executable == "ffmpeg") return "/usr/bin/ffmpeg";
    if (executable == "echo") return "/bin/echo";
    if (executable == "cat") return "/bin/cat";
    
    return "";
}

void MockSubprocess::set_result_for_command(const std::vector<std::string>& command, 
                                           const SubprocessResult& result) {
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

void MockSubprocess::add_result_queue(const std::vector<std::string>& command, 
                                     std::queue<SubprocessResult> results) {
    command_result_queues_[command] = std::move(results);
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
        return {{}, "", "", 0};
    }
    return call_history_.back();
}

SubprocessResult MockSubprocess::get_result_for_command(const std::vector<std::string>& command) {
    // Check for queued results first
    auto queue_it = command_result_queues_.find(command);
    if (queue_it != command_result_queues_.end() && !queue_it->second.empty()) {
        auto result = queue_it->second.front();
        queue_it->second.pop();
        return result;
    }
    
    // Check for specific command result
    auto it = command_results_.find(command);
    if (it != command_results_.end()) {
        return it->second;
    }
    
    // Generate realistic output for ffmpeg commands
    if (!command.empty()) {
        if (command[0] == "ffmpeg") {
            if (std::find(command.begin(), command.end(), "-encoders") != command.end()) {
                return {0, "h264,h265,vp8,vp9,av1", "", true, ""};
            }
            if (std::find(command.begin(), command.end(), "-hwaccels") != command.end()) {
                return {0, "cuda,vaapi,qsv", "", true, ""};
            }
            // Transcoding simulation
            return {0, "frame= 1000 fps= 30 q=23.0 size=    1024kB time=00:00:33.33", "", true, ""};
        }
    }
    
    return default_result_;
}

void MockSubprocess::record_call(const std::vector<std::string>& command, const std::string& stdin_input,
                                const std::string& working_directory, int timeout_seconds) {
    call_history_.push_back({command, stdin_input, working_directory, timeout_seconds});
}

std::string MockSubprocess::command_to_string(const std::vector<std::string>& command) const {
    std::ostringstream oss;
    for (size_t i = 0; i < command.size(); ++i) {
        if (i > 0) oss << " ";
        oss << command[i];
    }
    return oss.str();
}

} // namespace transcoding_engine