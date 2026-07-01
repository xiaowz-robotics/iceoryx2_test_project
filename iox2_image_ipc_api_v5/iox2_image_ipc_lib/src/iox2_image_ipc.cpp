#include "iox2_image_ipc.hpp"

#include "iox2/iceoryx2.hpp"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <exception>
#include <iostream>
#include <thread>

namespace ipc {
namespace {

using Clock = std::chrono::high_resolution_clock;

static constexpr uint64_t IOX2_IMAGE_MAGIC = 0x494F58325F494D47ULL; // "IOX2_IMG"-like marker

struct ImageWireHeader {
    uint64_t magic;
    uint64_t payload_size;
    uint64_t timestamp_ns;
    uint64_t reserved;
};

static constexpr uint64_t HEADER_SIZE = sizeof(ImageWireHeader);

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

static bool invalid_service_name(const char* service_name) {
    return service_name == nullptr || service_name[0] == '\0';
}

} // namespace

int ImageSender::iox2_image_send(const char* service_name,
                                 const std::vector<uint8_t>& image_data) {
    if (invalid_service_name(service_name)) {
        std::cerr << "[iox2_image_ipc] invalid service_name\n";
        return -1;
    }
    if (image_data.empty()) {
        std::cerr << "[iox2_image_ipc] image_data is empty\n";
        return -2;
    }

    try {
        using namespace iox2;
        set_log_level_from_env_or(LogLevel::Info);

        const uint64_t image_size = static_cast<uint64_t>(image_data.size());
        const uint64_t total_size = HEADER_SIZE + image_size;

        auto node = NodeBuilder().create<ServiceType::Ipc>().value();

        auto service = node.service_builder(ServiceName::create(service_name).value())
                           .publish_subscribe<bb::Slice<uint8_t>>()
                           .open_or_create()
                           .value();

        auto publisher = service.publisher_builder()
                                .initial_max_slice_len(total_size)
                                .allocation_strategy(AllocationStrategy::Static)
                                .create()
                                .value();

        ImageWireHeader hdr{};
        hdr.magic = IOX2_IMAGE_MAGIC;
        hdr.payload_size = image_size;
        hdr.timestamp_ns = 0;
        hdr.reserved = 0;

        auto sample = publisher.loan_slice_uninit(total_size).value();

        auto initialized_sample = std::move(sample).write_from_fn([&](auto i) -> uint8_t {
            if (i < HEADER_SIZE) {
                return reinterpret_cast<const uint8_t*>(&hdr)[i];
            }
            return image_data[static_cast<size_t>(i - HEADER_SIZE)];
        });

        // Write timestamp after payload preparation and immediately before send().
        const uint64_t send_ts = now_ns();
        uint8_t* shm_ptr = initialized_sample.payload_mut().begin();
        std::memcpy(shm_ptr + offsetof(ImageWireHeader, timestamp_ns),
                    &send_ts,
                    sizeof(send_ts));

        send(std::move(initialized_sample)).value();

        constexpr auto POST_SEND_KEEPALIVE = std::chrono::milliseconds(2000);
        std::this_thread::sleep_for(POST_SEND_KEEPALIVE);

        std::cout << "[ImageSender][iox2_image_send] service=" << service_name
                  << " bytes=" << image_size
                  << " timestamp_ns=" << send_ts
                  << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[iox2_image_ipc] ImageSender::iox2_image_send exception: "
                  << e.what() << std::endl;
        return -3;
    } catch (...) {
        std::cerr << "[iox2_image_ipc] ImageSender::iox2_image_send unknown exception\n";
        return -3;
    }
}

int ImageReceiver::iox2_image_receive_with_callback(const char* service_name,
                                                    uint64_t timeout_ms,
                                                    Iox2ImageReceiveCallback callback) {
    if (invalid_service_name(service_name) || callback == nullptr) {
        std::cerr << "[iox2_image_ipc] invalid argument\n";
        return -1;
    }

    try {
        using namespace iox2;
        set_log_level_from_env_or(LogLevel::Info);

        auto node = NodeBuilder().create<ServiceType::Ipc>().value();

        auto service = node.service_builder(ServiceName::create(service_name).value())
                           .publish_subscribe<bb::Slice<uint8_t>>()
                           .open_or_create()
                           .value();

        auto subscriber = service.subscriber_builder().create().value();

        const auto start = Clock::now();
        constexpr auto POLL_SLEEP = std::chrono::milliseconds(1);

        while (true) {
            auto result = subscriber.receive().value();
            if (result.has_value()) {
                const uint64_t recv_ts = now_ns();

                auto& sample = *result;
                auto payload = sample.payload();
                const uint8_t* raw = payload.data();
                const uint64_t raw_len = payload.number_of_bytes();

                if (raw_len < HEADER_SIZE) {
                    std::cerr << "[iox2_image_ipc] invalid payload: too small, bytes="
                              << raw_len << std::endl;
                    return -5;
                }

                ImageWireHeader hdr{};
                std::memcpy(&hdr, raw, HEADER_SIZE);

                if (hdr.magic != IOX2_IMAGE_MAGIC) {
                    std::cerr << "[iox2_image_ipc] invalid payload: bad magic\n";
                    return -5;
                }

                if (hdr.payload_size + HEADER_SIZE > raw_len) {
                    std::cerr << "[iox2_image_ipc] invalid payload: size mismatch, hdr="
                              << hdr.payload_size << " raw=" << raw_len << std::endl;
                    return -5;
                }

                const double latency_ms = hdr.timestamp_ns == 0
                    ? -1.0
                    : static_cast<double>(recv_ts - hdr.timestamp_ns) / 1e6;

                const uint8_t* image_ptr = raw + HEADER_SIZE;
                std::vector<uint8_t> image_data(image_ptr, image_ptr + hdr.payload_size);

                std::cout << "[ImageReceiver][iox2_image_receive_with_callback] service=" << service_name
                          << " bytes=" << hdr.payload_size
                          << " send_ts=" << hdr.timestamp_ns
                          << " recv_ts=" << recv_ts
                          << " latency_ms=" << latency_ms
                          << std::endl;

                const int cb_ret = callback(image_data);
                if (cb_ret != 0) {
                    std::cerr << "[iox2_image_ipc] callback returned error: " << cb_ret << std::endl;
                    return -6;
                }

                return 0;
            }

            if (timeout_ms != 0) {
                const auto now = Clock::now();
                const auto elapsed_ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (elapsed_ms >= static_cast<int64_t>(timeout_ms)) {
                    std::cerr << "[iox2_image_ipc] receive timeout after " << timeout_ms << " ms\n";
                    return -4;
                }
            }

            std::this_thread::sleep_for(POLL_SLEEP);
        }
    } catch (const std::exception& e) {
        std::cerr << "[iox2_image_ipc] ImageReceiver::iox2_image_receive_with_callback exception: "
                  << e.what() << std::endl;
        return -3;
    } catch (...) {
        std::cerr << "[iox2_image_ipc] ImageReceiver::iox2_image_receive_with_callback unknown exception\n";
        return -3;
    }
}

} // namespace ipc
