// ros_image_publisher.cpp
// ROS 2 Jazzy image stress publisher
//
// Usage example:
//   ros2 run ros2_image_stress_test ros_image_publisher --ros-args \
//     -p sender_id:=1 \
//     -p rounds:=1000 \
//     -p interval_ms:=5
//
// Each sender publishes one image:
//   sender_id=1  -> /home/ubuntu/vla/test_image/1/1.png
//   sender_id=2  -> /home/ubuntu/vla/test_image/1/2.png
//   ...
//   sender_id=10 -> /home/ubuntu/vla/test_image/1/10.png

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        std::cerr << "[ROS2 Publisher] Cannot open image: "
                  << path << std::endl;
        return {};
    }

    return std::vector<uint8_t>(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>());
}

static int64_t now_ns_system() {
    rclcpp::Clock clock(RCL_SYSTEM_TIME);
    return clock.now().nanoseconds();
}

class ImageStressPublisher : public rclcpp::Node {
public:
    ImageStressPublisher()
        : Node("ros_image_publisher") {
        sender_id_ = this->declare_parameter<int>("sender_id", 1);
        rounds_ = this->declare_parameter<int>("rounds", 100);
        interval_ms_ = this->declare_parameter<int>("interval_ms", 5);
        image_dir_ = this->declare_parameter<std::string>(
            "image_dir", "/home/ubuntu/vla/test_image/1");
        topic_prefix_ = this->declare_parameter<std::string>(
            "topic_prefix", "/ros2_image_stress/image_");
        reliable_ = this->declare_parameter<bool>("reliable", true);

        if (sender_id_ < 1 || sender_id_ > 10) {
            throw std::runtime_error("sender_id must be in range [1, 10]");
        }

        image_path_ = image_dir_ + "/" + std::to_string(sender_id_) + ".png";
        image_data_ = read_file(image_path_);

        if (image_data_.empty()) {
            throw std::runtime_error("image file is empty or cannot be read: " + image_path_);
        }

        topic_name_ = topic_prefix_ + std::to_string(sender_id_);

        auto qos = rclcpp::QoS(rclcpp::KeepLast(32));
        if (reliable_) {
            qos.reliable();
        } else {
            qos.best_effort();
        }
        qos.durability_volatile();

        publisher_ = this->create_publisher<sensor_msgs::msg::CompressedImage>(
            topic_name_, qos);

        std::cout << "======================================\n";
        std::cout << " ROS2 Image Publisher\n";
        std::cout << "======================================\n";
        std::cout << " Sender ID   : " << sender_id_ << "\n";
        std::cout << " PID         : " << getpid() << "\n";
        std::cout << " Topic       : " << topic_name_ << "\n";
        std::cout << " Image       : " << image_path_ << "\n";
        std::cout << " Image Size  : " << std::fixed << std::setprecision(2)
                  << static_cast<double>(image_data_.size()) / (1024.0 * 1024.0)
                  << " MB (" << image_data_.size() << " bytes)\n";
        std::cout << " Rounds      : " << rounds_ << "\n";
        std::cout << " Interval    : " << interval_ms_ << " ms\n";
        std::cout << " QoS         : " << (reliable_ ? "reliable" : "best_effort") << "\n";
        std::cout << "======================================\n";
    }

    void run() {
        // Give subscriber side enough time to discover/match.
        std::cout << "[ROS2 Publisher " << sender_id_
                  << "][PID " << getpid()
                  << "] waiting 2s for discovery...\n";
        std::this_thread::sleep_for(2s);

        int sent_count = 0;
        double total_publish_us = 0.0;

        for (int seq = 0; seq < rounds_ && rclcpp::ok(); ++seq) {
            sensor_msgs::msg::CompressedImage msg;

            msg.format = "png";

            // frame_id carries sender_id and seq so subscriber can validate.
            // Format:
            //   sender=<sender_id>;seq=<seq>
            msg.header.frame_id =
                "sender=" + std::to_string(sender_id_) +
                ";seq=" + std::to_string(seq);

            // Copy image bytes into ROS message buffer.
            // This is normal ROS2 message mode, not iceoryx2 zero-copy.
            msg.data = image_data_;

            // Record timestamp immediately before publish().
            const int64_t send_ts_ns = now_ns_system();
            msg.header.stamp = rclcpp::Time(send_ts_ns, RCL_SYSTEM_TIME);

            const auto t0 = std::chrono::high_resolution_clock::now();
            publisher_->publish(msg);
            const auto t1 = std::chrono::high_resolution_clock::now();

            const double publish_us =
                std::chrono::duration<double, std::micro>(t1 - t0).count();

            total_publish_us += publish_us;
            sent_count++;

            std::cout << "[ROS2 Publisher " << sender_id_
                      << "][PID " << getpid()
                      << "][SEND] "
                      << "seq=" << std::setw(5) << seq
                      << " bytes=" << msg.data.size()
                      << " send_ts=" << send_ts_ns
                      << " publish_call=" << std::fixed << std::setprecision(3)
                      << publish_us << " us"
                      << std::endl;

            if (interval_ms_ > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
            }
        }

        std::cout << "[ROS2 Publisher " << sender_id_
                  << "][PID " << getpid()
                  << "][SUMMARY] "
                  << "sent=" << sent_count << "/" << rounds_
                  << " avg_publish_call="
                  << std::fixed << std::setprecision(3)
                  << (sent_count > 0 ? total_publish_us / sent_count : 0.0)
                  << " us"
                  << std::endl;
    }

private:
    int sender_id_{1};
    int rounds_{100};
    int interval_ms_{5};
    bool reliable_{true};

    std::string image_dir_;
    std::string image_path_;
    std::string topic_prefix_;
    std::string topic_name_;

    std::vector<uint8_t> image_data_;

    rclcpp::Publisher<sensor_msgs::msg::CompressedImage>::SharedPtr publisher_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);

    try {
        auto node = std::make_shared<ImageStressPublisher>();
        node->run();
    } catch (const std::exception& e) {
        std::cerr << "[ROS2 Publisher][FATAL] "
                  << e.what() << std::endl;
        rclcpp::shutdown();
        return 1;
    }

    rclcpp::shutdown();
    return 0;
}