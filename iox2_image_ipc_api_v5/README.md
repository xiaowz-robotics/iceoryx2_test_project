# iox2_image_ipc_api_v5

This version keeps the original API function names, parameters and return values,
but changes the API form from free functions to class methods.

## Module structure

```text
iox2_image_ipc_api_v5/
├── build_all.sh
├── README.md
├── iox2_image_ipc_lib/      # independent shared library module
├── sender_app/              # independent sender app module
└── receiver_app/            # independent receiver app module
```

## Public API

```cpp
namespace ipc {

using Iox2ImageReceiveCallback = int (*)(const std::vector<uint8_t>& image_data);

class ImageSender {
public:
    int iox2_image_send(const char* service_name,
                        const std::vector<uint8_t>& image_data);
};

class ImageReceiver {
public:
    int iox2_image_receive_with_callback(const char* service_name,
                                         uint64_t timeout_ms,
                                         Iox2ImageReceiveCallback callback);
};

}
```

## App usage

### sender_app

No command-line arguments. It reads:

```text
/home/ubuntu/vla/test_image/1/1.png
```

and calls:

```cpp
ipc::ImageSender sender;
sender.iox2_image_send(SERVICE_NAME.c_str(), image);
```

### receiver_app

No command-line arguments. It waits for one image and saves it to:

```text
/home/ubuntu/IPC/received_from_image_sender_class_api_v5.png
```

and calls:

```cpp
ipc::ImageReceiver receiver;
receiver.iox2_image_receive_with_callback(
    SERVICE_NAME.c_str(),
    TIMEOUT_MS,
    save_image_callback);
```

## Build

```bash
cd ~/IPC
# tar -xzf iox2_image_ipc_api_v5.tar.gz
cd iox2_image_ipc_api_v5
./build_all.sh
```

If iceoryx2 is installed somewhere else:

```bash
IOX2_INSTALL=/path/to/iceoryx2/install ./build_all.sh
```

## Run

Terminal 1:

```bash
cd ~/IPC/iox2_image_ipc_api_v5/receiver_app/build
./receiver_app
```

Terminal 2:

```bash
cd ~/IPC/iox2_image_ipc_api_v5/sender_app/build
./sender_app
```
