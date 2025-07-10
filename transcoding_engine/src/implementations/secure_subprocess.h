#ifndef SECURE_SUBPROCESS_H
#define SECURE_SUBPROCESS_H

#include "../interfaces/subprocess_interface.h"
#include <memory>

namespace transcoding_engine {

class SecureSubprocess : public ISubprocessRunner {
public:
    SecureSubprocess();
    ~SecureSubprocess() override;
    
    SubprocessResult run(const std::vector<std::string>& command,
                        const std::string& working_directory = "",
                        int timeout_seconds = 0) override;
    
    SubprocessResult run_with_input(const std::vector<std::string>& command,
                                   const std::string& stdin_input,
                                   const std::string& working_directory = "",
                                   int timeout_seconds = 0) override;
    
    bool is_executable_available(const std::string& executable) override;
    std::string find_executable_path(const std::string& executable) override;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace transcoding_engine

#endif // SECURE_SUBPROCESS_H