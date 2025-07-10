#ifndef MOCK_SUBPROCESS_H
#define MOCK_SUBPROCESS_H

#include "../interfaces/subprocess_interface.h"
#include <map>
#include <queue>

namespace transcoding_engine {

class MockSubprocess : public ISubprocessRunner {
public:
    MockSubprocess() = default;
    ~MockSubprocess() override = default;
    
    // ISubprocessRunner interface
    SubprocessResult run(const std::vector<std::string>& command,
                        const std::string& working_directory = "",
                        int timeout_seconds = 0) override;
    
    SubprocessResult run_with_input(const std::vector<std::string>& command,
                                   const std::string& stdin_input,
                                   const std::string& working_directory = "",
                                   int timeout_seconds = 0) override;
    
    bool is_executable_available(const std::string& executable) override;
    std::string find_executable_path(const std::string& executable) override;
    
    // Mock-specific methods
    void set_result_for_command(const std::vector<std::string>& command, const SubprocessResult& result);
    void set_default_result(const SubprocessResult& result);
    void set_executable_available(const std::string& executable, bool available);
    void set_executable_path(const std::string& executable, const std::string& path);
    void add_result_queue(const std::vector<std::string>& command, std::queue<SubprocessResult> results);
    
    // Call tracking
    struct CallInfo {
        std::vector<std::string> command;
        std::string stdin_input;
        std::string working_directory;
        int timeout_seconds;
    };
    
    const std::vector<CallInfo>& get_call_history() const { return call_history_; }
    size_t get_call_count() const { return call_history_.size(); }
    void clear_call_history() { call_history_.clear(); }
    
    bool was_command_called(const std::vector<std::string>& command) const;
    bool was_executable_called(const std::string& executable) const;
    CallInfo get_last_call() const;
    
    void clear_mock_data() {
        command_results_.clear();
        command_result_queues_.clear();
        executable_availability_.clear();
        executable_paths_.clear();
        call_history_.clear();
    }

private:
    std::map<std::vector<std::string>, SubprocessResult> command_results_;
    std::map<std::vector<std::string>, std::queue<SubprocessResult>> command_result_queues_;
    std::map<std::string, bool> executable_availability_;
    std::map<std::string, std::string> executable_paths_;
    SubprocessResult default_result_{0, "", "", true, ""};
    std::vector<CallInfo> call_history_;
    
    SubprocessResult get_result_for_command(const std::vector<std::string>& command);
    void record_call(const std::vector<std::string>& command, const std::string& stdin_input,
                    const std::string& working_directory, int timeout_seconds);
    std::string command_to_string(const std::vector<std::string>& command) const;
};

} // namespace transcoding_engine

#endif // MOCK_SUBPROCESS_H