#include "iox2_image_ipc.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

const std::string SERVICE_NAME = ipc::kDefaultServiceName;
const std::string OUTPUT_PATH = "/home/ubuntu/IPC/received_from_image_sender_class_api_v5.png";
constexpr uint64_t TIMEOUT_MS = 30000;

void make_dirs_for_file(const std::string& file_path) {
    const auto pos = file_path.find_last_of('/');
    if (pos == std::string::npos) {
        return;
    }

    std::string dir = file_path.substr(0, pos);
    if (dir.empty()) {
        return;
    }

    for (size_t i = 1; i < dir.size(); ++i) {
        if (dir[i] == '/') {
            dir[i] = '\0';
            mkdir(dir.c_str(), 0755);
            dir[i] = '/';
        }
    }
    mkdir(dir.c_str(), 0755);
}

int save_image_callback(const std::vector<uint8_t>& image_data) {
    make_dirs_for_file(OUTPUT_PATH);

    std::ofstream ofs(OUTPUT_PATH, std::ios::binary);
    if (!ofs) {
        std::cerr << "[receiver_app] Cannot open output file: " << OUTPUT_PATH << std::endl;
        return -1;
    }

    ofs.write(reinterpret_cast<const char*>(image_data.data()),
              static_cast<std::streamsize>(image_data.size()));
    ofs.close();

    std::cout << "[receiver_app][CALLBACK] saved bytes=" << image_data.size()
              << " to " << OUTPUT_PATH << std::endl;
    return 0;
}

} // namespace

int main() {
    std::cout << "======================================\n";
    std::cout << " receiver_app: class-style API demo\n";
    std::cout << "======================================\n";
    std::cout << " PID     : " << getpid() << "\n";
    std::cout << " Service : " << SERVICE_NAME << "\n";
    std::cout << " Output  : " << OUTPUT_PATH << "\n";
    std::cout << " Timeout : " << TIMEOUT_MS << " ms\n";
    std::cout << "======================================\n";
    std::cout << "[receiver_app] waiting for image...\n";

    // New class-style API; method name, parameters and return value are unchanged.
    ipc::ImageReceiver receiver;
    const int ret = receiver.iox2_image_receive_with_callback(
        SERVICE_NAME.c_str(),
        TIMEOUT_MS,
        save_image_callback);

    if (ret != 0) {
        std::cerr << "[receiver_app] receiver.iox2_image_receive_with_callback() failed, ret="
                  << ret << std::endl;
        return ret;
    }

    std::cout << "[receiver_app] receive success\n";
    return 0;
}
