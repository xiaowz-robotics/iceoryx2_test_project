#include "iox2_image_ipc.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

const std::string SERVICE_NAME = ipc::kDefaultServiceName;
const std::string IMAGE_PATH = "/home/ubuntu/vla/test_image/1/1.png";

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "[sender_app] Cannot open image: " << path << std::endl;
        return {};
    }

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    std::cout << "======================================\n";
    std::cout << " sender_app: class-style API demo\n";
    std::cout << "======================================\n";
    std::cout << " PID     : " << getpid() << "\n";
    std::cout << " Service : " << SERVICE_NAME << "\n";
    std::cout << " Image   : " << IMAGE_PATH << "\n";
    std::cout << "======================================\n";

    auto image = read_file(IMAGE_PATH);
    if (image.empty()) {
        return 1;
    }

    std::cout << "[sender_app] Read image bytes: " << image.size()
              << " (" << std::fixed << std::setprecision(2)
              << image.size() / (1024.0 * 1024.0) << " MB)\n";

    // New class-style API; method name, parameters and return value are unchanged.
    ipc::ImageSender sender;
    const int ret = sender.iox2_image_send(SERVICE_NAME.c_str(), image);
    if (ret != 0) {
        std::cerr << "[sender_app] sender.iox2_image_send() failed, ret=" << ret << std::endl;
        return ret;
    }

    std::cout << "[sender_app] send success\n";
    return 0;
}
