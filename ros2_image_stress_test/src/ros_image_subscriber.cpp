// ros_image_subscriber.cpp
// ROS 2 Jazzy image stress subscriber
//
// Usage example:
//   ros2 run ros2_image_stress_test ros_image_subscriber --ros-args \
//     -p receiver_id:=1 \
//     -p expected_count:=1000 \
//     -p timeout_sec:=120
//
// Each receiver subscribes to:
//   /ros2_image_stress/image_<receiver_id>
//
// It does not save images.
// It prints latency and receive count.

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/compressed_image.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>
#include <unistd.h>

using namespace std::chrono_literals;

static int64_t now_ns_system() {
    rclcpp::Clock clock(RCL_SYSTEM_TIME);
    return clock.now().nanoseconds();
}

static bool parse_frame_id(
    const std::string& frame_id,
    int& sender_id,
    int& seq) {
    // Expected:
    //   sender=<id>;seq=<seq>
    static const std::regex re(R"(sender=([0-9]+);seq=([0-9]+))");

    std::smatch m;
    if (!std::regex_match(frame_id, m, re)) {
        return false;
    }

    sender_id = std::stoi(m[1].str());
    seq = std::stoi(m[2].str());
    return true;
}

class ImageStressSubscriber : public rclcpp::Node {
public:
    ImageStressSubscriber()
        : Node("ros_image_subscriber") {
        receiver_id_ = this->declare_parameter<int>("receiver_id", 1);
        expected_count_ = this->declare_parameter<int>("expected_count", 100);
        timeout_sec_ = this->declare_parameter<int>("timeout_sec", 120);
        topic_prefix_ = this->declare_parameter<std::string>(
            "topic_prefix", "/ros2_image_stress/image_");
        reliable_ = this->declare_parameter<bool>("reliable", true);

        if (receiver_id_ < 1 || receiver_id_ > 10) {
            throw std::runtime_error("receiver_id must be in range [1, 10]");
        }

        topic_name_ = topic_prefix_ + std::to_string(receiver_id_);

        auto qos = rclcpp::QoS(rclcpp::KeepLast(32));
        if (reliable_) {
            qos.reliable();
        } else {
            qos.best_effort();
        }
        qos.durability_volatile();

        subscriber_ = this->create_subscription<sensor_msgs::msg::CompressedImage>(
            topic_name_,
            qos,
            [this](sensor_msgs::msg::CompressedImage::ConstSharedPtr msg) {
                this->on_msg(msg);
            });

        start_time_ = std::chrono::steady_clock::now();

        timer_ = this->create_wall_timer(
            500ms,
            [this]() {
                const auto elapsed_sec =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - start_time_)
                        .count();

                if (received_count_.load() >= expected_count_) {
                    print_summary("complete");
                    rclcpp::shutdown();
                    return;
                }

                if (elapsed_sec >= timeout_sec_) {
                    print_summary("timeout");
                    rclcpp::shutdown();
                    return;
                }
            });

        std::cout << "======================================\n";
        std::cout << " ROS2 Image Subscriber\n";
        std::cout << "======================================\n";
        std::cout << " Receiver ID    : " << receiver_id_ << "\n";
        std::cout << " PID            : " << getpid() << "\n";
        std::cout << " Topic          : " << topic_name_ << "\n";
        std::cout << " Expected Count : " << expected_count_ << "\n";
        std::cout << " Timeout        : " << timeout_sec_ << " sec\n";
        std::cout << " QoS            : " << (reliable_ ? "reliable" : "best_effort") << "\n";
        std::cout << "======================================\n";
    }

private:
    void on_msg(const sensor_msgs::msg::CompressedImage::ConstSharedPtr& msg) {
        // Record receive timestamp as early as possible.
        const int64_t recv_ts_ns = now_ns_system();
        const int64_t send_ts_ns =
            rclcpp::Time(msg->header.stamp).nanoseconds();

        const double latency_ms =
            static_cast<double>(recv_ts_ns - send_ts_ns) / 1e6;

        int sender_id = -1;
        int seq = -1;

        const bool parsed =
            parse_frame_id(msg->header.frame_id, sender_id, seq);

        const bool ok =
            parsed &&
            sender_id == receiver_id_ &&
            !msg->data.empty();

        const int count = ++received_count_;

        total_latency_ms_ += latency_ms;

        if (latency_ms < min_latency_ms_) {
            min_latency_ms_ = latency_ms;
        }

        if (latency_ms > max_latency_ms_) {
            max_latency_ms_ = latency_ms;
        }

        std::cout << "[ROS2 Subscriber " << receiver_id_
                  << "][PID " << getpid()
                  << "][RECV] "
                  << "count=" << std::setw(5) << count
                  << "/" << expected_count_
                  << " sender=" << sender_id
                  << " seq=" << std::setw(5) << seq
                  << " bytes=" << msg->data.size()
                  << " send_ts=" << send_ts_ns
                  << " recv_ts=" << recv_ts_ns
                  << " latency=" << std::fixed << std::setprecision(3)
                  << latency_ms << " ms"
                  << " ok=" << (ok ? "Y" : "N")
                  << std::endl;
    }

    void print_summary(const std::string& reason) {
        if (summary_printed_.exchange(true)) {
            return;
        }

        const int count = received_count_.load();

        const double avg_latency =
            count > 0
                ? total_latency_ms_ / static_cast<double>(count)
                : 0.0;

        std::cout << "[ROS2 Subscriber " << receiver_id_
                  << "][PID " << getpid()
                  << "][SUMMARY] "
                  << "reason=" << reason
                  << " received=" << count << "/" << expected_count_
                  << " avg_latency=" << std::fixed << std::setprecision(3)
                  << avg_latency << " ms"
                  << " min_latency=" << min_latency_ms_ << " ms"
                  << " max_latency=" << max_latency_ms_ << " ms"
                  << std::endl;
    }

private:
    int receiver_id_{1};
    int expected_count_{100};
    int timeout_sec_{120};
    bool reliable_{true};

    std::string topic_prefix_;
    std::string topic_name_;

    std::atomic<int> received_count_{0};
    std::atomic<bool> summary_printed_{false};

    double total_latency_ms_{0.0};
    double min_latency_ms_{1e30};
    double max_latency_ms_{0.0};

    std::chrono::steady_clock::time_point start_time_;

    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr subscriber_;
    rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);

    try {
        auto node = std::make_shared<ImageStressSubscriber>();
        rclcpp::spin(node);
    } catch (const std::exception& e) {
        std::cerr << "[ROS2 Subscriber][FATAL] "
                  << e.what() << std::endl;
        rclcpp::shutdown();
        return 1;
    }

    return 0;
}