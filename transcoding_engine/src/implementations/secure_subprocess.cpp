#include "secure_subprocess.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <filesystem>
#include <chrono>
#include <thread>

namespace transcoding_engine {

class SecureSubprocess::Impl {
public:
    SubprocessResult execute_command(const std::vector<std::string>& command,
                                   const std::string& stdin_input,
                                   const std::string& working_directory,
                                   int timeout_seconds) {
        if (command.empty()) {
            return {-1, "", "", false, "Empty command"};
        }
        
        // Validate executable exists
        if (!is_executable_available_impl(command[0])) {
            return {-1, "", "", false, "Executable not found: " + command[0]};
        }
        
        // Create pipes for stdout, stderr, and stdin
        int stdout_pipe[2], stderr_pipe[2], stdin_pipe[2];
        
        if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1 || pipe(stdin_pipe) == -1) {
            return {-1, "", "", false, "Failed to create pipes"};
        }
        
        pid_t pid = fork();
        
        if (pid == -1) {
            // Fork failed
            close(stdout_pipe[0]); close(stdout_pipe[1]);
            close(stderr_pipe[0]); close(stderr_pipe[1]);
            close(stdin_pipe[0]); close(stdin_pipe[1]);
            return {-1, "", "", false, "Failed to fork process"};
        }
        
        if (pid == 0) {
            // Child process
            execute_child_process(command, stdout_pipe, stderr_pipe, stdin_pipe, working_directory);
            _exit(127); // Should never reach here
        }
        
        // Parent process
        return handle_parent_process(pid, stdout_pipe, stderr_pipe, stdin_pipe, 
                                   stdin_input, timeout_seconds);
    }
    
private:
    void execute_child_process(const std::vector<std::string>& command,
                             int stdout_pipe[2], int stderr_pipe[2], int stdin_pipe[2],
                             const std::string& working_directory) {
        // Close parent ends of pipes
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        close(stdin_pipe[1]);
        
        // Redirect stdout, stderr, stdin
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        dup2(stdin_pipe[0], STDIN_FILENO);
        
        // Close pipe file descriptors
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stdin_pipe[0]);
        
        // Change working directory if specified
        if (!working_directory.empty()) {
            if (chdir(working_directory.c_str()) != 0) {
                std::cerr << "Failed to change directory to: " << working_directory << std::endl;
                _exit(127);
            }
        }
        
        // Prepare arguments for execvp
        std::vector<char*> args;
        for (const auto& arg : command) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }
        args.push_back(nullptr);
        
        // Execute the command
        execvp(args[0], args.data());
        
        // If we reach here, execvp failed
        std::cerr << "Failed to execute: " << command[0] << std::endl;
        _exit(127);
    }
    
    SubprocessResult handle_parent_process(pid_t pid, int stdout_pipe[2], int stderr_pipe[2], 
                                         int stdin_pipe[2], const std::string& stdin_input,
                                         int timeout_seconds) {
        // Close child ends of pipes
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stdin_pipe[0]);
        
        // Write stdin input if provided
        if (!stdin_input.empty()) {
            write(stdin_pipe[1], stdin_input.c_str(), stdin_input.length());
        }
        close(stdin_pipe[1]);
        
        // Read outputs
        std::string stdout_output = read_from_pipe(stdout_pipe[0]);
        std::string stderr_output = read_from_pipe(stderr_pipe[0]);
        
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        // Wait for child process with timeout
        int status;
        bool timed_out = false;
        
        if (timeout_seconds > 0) {
            timed_out = !wait_with_timeout(pid, &status, timeout_seconds);
        } else {
            waitpid(pid, &status, 0);
        }
        
        if (timed_out) {
            kill(pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            return {-1, stdout_output, stderr_output, false, "Process timed out"};
        }
        
        int exit_code = WEXITSTATUS(status);
        bool success = (exit_code == 0);
        
        return {exit_code, stdout_output, stderr_output, success, ""};
    }
    
    std::string read_from_pipe(int pipe_fd) {
        std::ostringstream output;
        char buffer[4096];
        ssize_t bytes_read;
        
        while ((bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            output << buffer;
        }
        
        return output.str();
    }
    
    bool wait_with_timeout(pid_t pid, int* status, int timeout_seconds) {
        auto start_time = std::chrono::steady_clock::now();
        auto timeout_duration = std::chrono::seconds(timeout_seconds);
        
        while (true) {
            pid_t result = waitpid(pid, status, WNOHANG);
            
            if (result == pid) {
                return true; // Process finished
            }
            
            if (result == -1) {
                return false; // Error
            }
            
            auto current_time = std::chrono::steady_clock::now();
            if (current_time - start_time >= timeout_duration) {
                return false; // Timeout
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
public:
    bool is_executable_available_impl(const std::string& executable) {
        // Check if it's an absolute path
        if (executable[0] == '/') {
            return std::filesystem::exists(executable) && 
                   std::filesystem::is_regular_file(executable);
        }
        
        // Search in PATH
        const char* path_env = getenv("PATH");
        if (!path_env) {
            return false;
        }
        
        std::string path_str(path_env);
        std::istringstream path_stream(path_str);
        std::string path_dir;
        
        while (std::getline(path_stream, path_dir, ':')) {
            std::filesystem::path full_path = std::filesystem::path(path_dir) / executable;
            if (std::filesystem::exists(full_path) && 
                std::filesystem::is_regular_file(full_path)) {
                return true;
            }
        }
        
        return false;
    }
    
    std::string find_executable_path_impl(const std::string& executable) {
        // Check if it's an absolute path
        if (executable[0] == '/') {
            if (std::filesystem::exists(executable) && 
                std::filesystem::is_regular_file(executable)) {
                return executable;
            }
            return "";
        }
        
        // Search in PATH
        const char* path_env = getenv("PATH");
        if (!path_env) {
            return "";
        }
        
        std::string path_str(path_env);
        std::istringstream path_stream(path_str);
        std::string path_dir;
        
        while (std::getline(path_stream, path_dir, ':')) {
            std::filesystem::path full_path = std::filesystem::path(path_dir) / executable;
            if (std::filesystem::exists(full_path) && 
                std::filesystem::is_regular_file(full_path)) {
                return full_path.string();
            }
        }
        
        return "";
    }
};

SecureSubprocess::SecureSubprocess() : pimpl_(std::make_unique<Impl>()) {}

SecureSubprocess::~SecureSubprocess() = default;

SubprocessResult SecureSubprocess::run(const std::vector<std::string>& command,
                                     const std::string& working_directory,
                                     int timeout_seconds) {
    return pimpl_->execute_command(command, "", working_directory, timeout_seconds);
}

SubprocessResult SecureSubprocess::run_with_input(const std::vector<std::string>& command,
                                                 const std::string& stdin_input,
                                                 const std::string& working_directory,
                                                 int timeout_seconds) {
    return pimpl_->execute_command(command, stdin_input, working_directory, timeout_seconds);
}

bool SecureSubprocess::is_executable_available(const std::string& executable) {
    return pimpl_->is_executable_available_impl(executable);
}

std::string SecureSubprocess::find_executable_path(const std::string& executable) {
    return pimpl_->find_executable_path_impl(executable);
}

} // namespace transcoding_engine