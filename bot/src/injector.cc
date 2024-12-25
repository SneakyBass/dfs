#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fmt/base.h>
#include <fstream>
#include <netinet/in.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#include "injector.hh"

namespace dfs
{
    static pid_t get_dofus_process() {
        DIR *dir = opendir("/proc");
        if (!dir) {
            fmt::println(stderr, "Failed to open /proc");
            return -1;
        }

        dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            pid_t pid = -1;
            int res = sscanf(entry->d_name, "%d", &pid);

            if (res == 1) {
                char cmdline_file[1024] = {0};
                sprintf(cmdline_file, "/proc/%d/cmdline", pid);

                std::ifstream fd;
                fd.open(cmdline_file);
                std::stringstream buffer;
                buffer << fd.rdbuf();

                if (strstr(buffer.str().c_str(), "Dofus.x64") != 0) {
                    fmt::println("Dofus.x64 found with PID {}", pid);
                    closedir(dir);
                    return pid;
                }
            }
        }

        closedir(dir);

        return -1;
    }

    void Attach() {
        int child_pid = get_dofus_process();

        if (child_pid == -1) {
            fmt::println("Process not found");
            return;
        }

        // TODO: Read memory and shit
    }
} // namespace dfs
