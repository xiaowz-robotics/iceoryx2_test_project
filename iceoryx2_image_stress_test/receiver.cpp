
#include "iox2/iceoryx2.hpp"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <unistd.h>

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
        std::cerr << "Usage: " << argv[0] << " <receiver_id> <expected_count> <timeout_sec>" << std::endl;
        return 1;
    }

    uint32_t receiver_id = std::stoi(argv[1]);
    uint64_t expected = std::stoull(argv[2]);
    uint64_t timeout_sec = std::stoull(argv[3]);
    pid_t pid = getpid();

    using namespace iox2;
    auto node = NodeBuilder().create<ServiceType::Ipc>().value();
    auto service_name = "ImageStress/Sender" + std::to_string(receiver_id);
    auto service = node.service_builder(ServiceName::create(service_name.c_str()).value())
                       .publish_subscribe<bb::Slice<uint8_t>>()
                       .open_or_create().value();

    auto subscriber = service.subscriber_builder().create().value();
    uint64_t count = 0;
    auto start_time = Clock::now();

    while (count < expected) {
        auto result = subscriber.receive().value();
        if (result.has_value()) {
            uint64_t recv_ts = now_ns();
            auto& sample = *result;
            auto payload = sample.payload();
            
            if (payload.number_of_bytes() >= HEADER_SIZE) {
                const ImageHeader* hdr = reinterpret_cast<const ImageHeader*>(payload.data());
                bool ok = (hdr->magic == MAGIC && hdr->sender_id == receiver_id);
                double latency = (recv_ts - hdr->timestamp_ns) / 1e6;
                
                std::cout << "[Receiver " << receiver_id << "][PID " << pid << "][RECV] count=" << std::setw(4) << count + 1 
                          << "/" << expected << " sender=" << hdr->sender_id << " seq=" << hdr->sequence 
                          << " bytes=" << payload.number_of_bytes() << " send_ts=" << hdr->timestamp_ns 
                          << " recv_ts=" << recv_ts << " latency=" << std::fixed << std::setprecision(3) << latency << " ms "
                          << "ok=" << (ok ? "Y" : "N") << std::endl;
                count++;
            }
        } else {
            if (std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - start_time).count() > timeout_sec) {
                std::cerr << "[Receiver " << receiver_id << "][PID " << pid << "] Timeout reached." << std::endl;
                break;
            }
            std::this_thread::yield();
        }
    }

    std::cout << "SUMMARY_RECEIVER_" << receiver_id << ":" << count << "/" << expected << std::endl;
    return 0;
}
