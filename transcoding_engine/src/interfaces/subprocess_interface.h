#ifndef SUBPROCESS_INTERFACE_H
#define SUBPROCESS_INTERFACE_H

#include <string>
#include <vector>

namespace transcoding_engine {

struct SubprocessResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    bool success;
    std::string error_message;
};

class ISubprocessRunner {
public:
    virtual ~ISubprocessRunner() = default;
    
    virtual SubprocessResult run(const std::vector<std::string>& command,
                               const std::string& working_directory = "",
                               int timeout_seconds = 0) = 0;
    
    virtual SubprocessResult run_with_input(const std::vector<std::string>& command,
                                          const std::string& stdin_input,
                                          const std::string& working_directory = "",
                                          int timeout_seconds = 0) = 0;
    
    virtual bool is_executable_available(const std::string& executable) = 0;
    virtual std::string find_executable_path(const std::string& executable) = 0;
};

} // namespace transcoding_engine

#endif // SUBPROCESS_INTERFACE_H