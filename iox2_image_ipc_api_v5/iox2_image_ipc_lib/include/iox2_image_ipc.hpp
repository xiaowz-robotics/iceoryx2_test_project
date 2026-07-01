#pragma once

#include <cstdint>
#include <vector>

namespace ipc {

// Return values:
//   0   success
//  -1   invalid argument
//  -2   invalid/empty image data
//  -3   iceoryx2 runtime error / exception
//  -4   timeout
//  -5   invalid received payload
//  -6   callback returned error

using Iox2ImageReceiveCallback = int (*)(const std::vector<uint8_t>& image_data);

// Default service name used by the sample apps.
inline constexpr const char* kDefaultServiceName = "ImageApi/Service";

// New class-style API.
// Only the API ownership form changes from free functions to class methods.
// Function names, parameters and return values are kept unchanged.
class ImageSender {
public:
    ImageSender() = default;
    ~ImageSender() = default;

    ImageSender(const ImageSender&) = delete;
    ImageSender& operator=(const ImageSender&) = delete;
    ImageSender(ImageSender&&) noexcept = default;
    ImageSender& operator=(ImageSender&&) noexcept = default;

    int iox2_image_send(const char* service_name,
                        const std::vector<uint8_t>& image_data);
};

class ImageReceiver {
public:
    ImageReceiver() = default;
    ~ImageReceiver() = default;

    ImageReceiver(const ImageReceiver&) = delete;
    ImageReceiver& operator=(const ImageReceiver&) = delete;
    ImageReceiver(ImageReceiver&&) noexcept = default;
    ImageReceiver& operator=(ImageReceiver&&) noexcept = default;

    int iox2_image_receive_with_callback(const char* service_name,
                                         uint64_t timeout_ms,
                                         Iox2ImageReceiveCallback callback);
};

} // namespace ipc
