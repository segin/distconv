#include "secure_subprocess.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <filesystem>
#include <chrono>
#include <thread>
#include <poll.h>
#include <sys/syscall.h>

#ifndef SYS_pidfd_open
#ifdef __x86_64__
#define SYS_pidfd_open 434
#elif defined(__aarch64__)
#define SYS_pidfd_open 434
#endif
#endif

namespace distconv {
namespace TranscodingEngine {

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
    
    void set_nonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
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
            // Note: This write might block if input is larger than pipe buffer.
            // For a robust implementation, this should also be non-blocking and handled in the loop,
            // but for this task we focus on output blocking.
            // Just basic write for now as per original implementation.
            write(stdin_pipe[1], stdin_input.c_str(), stdin_input.length());
        }
        close(stdin_pipe[1]);
        
        // Set pipes to non-blocking mode
        fcntl(stdout_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(stderr_pipe[0], F_SETFL, O_NONBLOCK);

        std::string stdout_output;
        std::string stderr_output;
        
        struct pollfd fds[2];
        fds[0].fd = stdout_pipe[0];
        fds[0].events = POLLIN;
        fds[1].fd = stderr_pipe[0];
        fds[1].events = POLLIN;

        auto start_time = std::chrono::steady_clock::now();
        bool timed_out = false;
        char buffer[4096];

        while (fds[0].fd != -1 || fds[1].fd != -1) {
            int remaining_ms = -1;
            if (timeout_seconds > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time);
                remaining_ms = (timeout_seconds * 1000) - elapsed.count();
                if (remaining_ms <= 0) {
                    timed_out = true;
                    break;
                }
            }

            // We use a small poll timeout (e.g., 100ms) even if global timeout is larger
            // to allow periodic checks, though we could rely on remaining_ms directly.
            // Using remaining_ms directly is efficient.
            int ret = poll(fds, 2, remaining_ms);

            if (ret == -1) {
                if (errno == EINTR) continue;
                break; // Error
            }

            if (ret == 0 && timeout_seconds > 0) {
                timed_out = true; // Poll timed out
                break;
            }

            // Check stdout
            if (fds[0].fd != -1) {
                if (fds[0].revents & POLLIN) {
                    ssize_t bytes = read(fds[0].fd, buffer, sizeof(buffer));
                    if (bytes > 0) {
                        stdout_output.append(buffer, bytes);
                    } else if (bytes == 0) {
                        close(fds[0].fd);
                        fds[0].fd = -1;
                    }
                } else if (fds[0].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    // Try to read remaining data if any
                    ssize_t bytes;
                    while ((bytes = read(fds[0].fd, buffer, sizeof(buffer))) > 0) {
                        stdout_output.append(buffer, bytes);
                    }
                    close(fds[0].fd);
                    fds[0].fd = -1;
                }
            }

            // Check stderr
            if (fds[1].fd != -1) {
                if (fds[1].revents & POLLIN) {
                    ssize_t bytes = read(fds[1].fd, buffer, sizeof(buffer));
                    if (bytes > 0) {
                        stderr_output.append(buffer, bytes);
                    } else if (bytes == 0) {
                        close(fds[1].fd);
                        fds[1].fd = -1;
                    }
                } else if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    // Try to read remaining data
                    ssize_t bytes;
                    while ((bytes = read(fds[1].fd, buffer, sizeof(buffer))) > 0) {
                        stderr_output.append(buffer, bytes);
                    }
                    close(fds[1].fd);
                    fds[1].fd = -1;
                }
            }
        }

        // Close any remaining open pipes
        if (fds[0].fd != -1) close(fds[0].fd);
        if (fds[1].fd != -1) close(fds[1].fd);

        if (timed_out) {
            kill(pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            kill(pid, SIGKILL);
            int status;
            waitpid(pid, &status, 0);
            return {-1, stdout_output, stderr_output, false, "Process timed out"};
        }
        
        // Wait for child process (it should be finished or nearly finished)
        int status;
        waitpid(pid, &status, 0);

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
#if defined(__linux__) && defined(SYS_pidfd_open)
        int pidfd = syscall(SYS_pidfd_open, pid, 0);
        if (pidfd >= 0) {
            struct pollfd pfd;
            pfd.fd = pidfd;
            pfd.events = POLLIN;

            int ret = poll(&pfd, 1, timeout_seconds * 1000);
            close(pidfd);

            if (ret > 0) {
                if (waitpid(pid, status, 0) == pid) {
                    return true;
                }
            } else if (ret == 0) {
                return false;
            }
            // Fallback to polling loop if poll fails
        }
#endif

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

} // namespace TranscodingEngine
} // namespace distconv
