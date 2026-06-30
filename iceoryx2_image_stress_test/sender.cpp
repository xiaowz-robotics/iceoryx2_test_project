
#include "iox2/iceoryx2.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

struct ImageHeader {
    uint32_t magic;
    uint32_t sender_id;
    uint64_t sequence;
    uint64_t image_data_size;
    uint64_t timestamp_ns;
};

static constexpr uint32_t MAGIC = 0xDEADC0DE;
static constexpr uint64_t HEADER_SIZE = sizeof(ImageHeader);

static uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count());
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <sender_id> <rounds> <interval_ms>" << std::endl;
        return 1;
    }

    uint32_t sender_id = std::stoi(argv[1]);
    uint64_t rounds = std::stoull(argv[2]);
    uint64_t interval_ms = std::stoull(argv[3]);
    pid_t pid = getpid();

    std::string img_path = "/home/ubuntu/vla/test_image/1/" + std::to_string(sender_id) + ".png";
    std::ifstream ifs(img_path, std::ios::binary);
    if (!ifs) {
        std::cerr << "[Sender " << sender_id << "][PID " << pid << "] Failed to open " << img_path << std::endl;
        return 1;
    }
    std::vector<uint8_t> img_data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    uint64_t total_size = HEADER_SIZE + img_data.size();

    using namespace iox2;
    auto node = NodeBuilder().create<ServiceType::Ipc>().value();
    auto service_name = "ImageStress/Sender" + std::to_string(sender_id);
    auto service = node.service_builder(ServiceName::create(service_name.c_str()).value())
                       .publish_subscribe<bb::Slice<uint8_t>>()
                       .open_or_create().value();

    auto publisher = service.publisher_builder()
                            .initial_max_slice_len(total_size)
                            .create().value();

    for (uint64_t i = 0; i < rounds; ++i) {
        auto t_prep_0 = Clock::now();
        auto sample = publisher.loan_slice_uninit(total_size).value();
        
        ImageHeader hdr { MAGIC, sender_id, i, img_data.size(), 0 };
        auto init_s = std::move(sample).write_from_fn([&](auto idx) -> uint8_t {
            if (idx < HEADER_SIZE) return reinterpret_cast<const uint8_t*>(&hdr)[idx];
            return img_data[idx - HEADER_SIZE];
        });

        auto t_prep_1 = Clock::now();
        uint64_t send_ts = now_ns();
        uint8_t* shm_ptr = init_s.payload_mut().begin();
        std::memcpy(shm_ptr + offsetof(ImageHeader, timestamp_ns), &send_ts, 8);

        auto t_send_0 = Clock::now();
        send(std::move(init_s)).value();
        auto t_send_1 = Clock::now();

        double prep_ms = std::chrono::duration<double, std::milli>(t_prep_1 - t_prep_0).count();
        double send_us = std::chrono::duration<double, std::micro>(t_send_1 - t_send_0).count();

        std::cout << "[Sender " << sender_id << "][PID " << pid << "][SEND] seq=" << std::setw(4) << i 
                  << " payload=" << std::fixed << std::setprecision(2) << total_size/(1024.0*1024.0) << " MB "
                  << "send_ts=" << send_ts << " prepare=" << std::setprecision(3) << prep_ms << " ms "
                  << "send_call=" << std::setprecision(1) << send_us << " us" << std::endl;

        if (interval_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    return 0;
}
