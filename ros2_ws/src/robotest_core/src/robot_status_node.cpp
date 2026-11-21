#include <chrono>
#include <memory>
#include <string>
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
using namespace std::chrono_literals;
class RobotStatusNode : public rclcpp::Node
{
public:
   RobotStatusNode()
       : Node("robot_status_node")
   {
       publisher_ =
           create_publisher<std_msgs::msg::String>(
               "/robotest/robot/status", 10);
       timer_ = create_wall_timer(
           1s,
           [this]()
           {
               std_msgs::msg::String message;
               message.data = "SIMULATION | RoArm-M2-S | DISCONNECTED";
               publisher_->publish(message);
               RCLCPP_INFO(
                   get_logger(),
                   "%s",
                   message.data.c_str());
           });
       RCLCPP_INFO(
           get_logger(),
           "RoboTest Studio status node started");
   }
private:
   rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
   rclcpp::TimerBase::SharedPtr timer_;
};
int main(int argc, char **argv)
{
   rclcpp::init(argc, argv);
   rclcpp::spin(
       std::make_shared<RobotStatusNode>());
   rclcpp::shutdown();
   return 0;
}
