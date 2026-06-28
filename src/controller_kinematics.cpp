// ===========================================================================
//  controller_kinematics.cpp
//  author: Koji Shibata  (ROS 2 port)
//  e-mail: kshibata.0519@gmail.com
//
//  Inverse-kinematics controller for the omnidirectional four-wheeled
//  (swerve-drive) robot.
//
//  Subscribes : /cmd_vel                       (geometry_msgs/msg/Twist)
//  Publishes  : /position_controller/commands  (std_msgs/msg/Float64MultiArray)
//                   -> steering angle (delta) of the 4 wheels
//               /velocity_controller/commands  (std_msgs/msg/Float64MultiArray)
//                   -> rotation speed (omega)  of the 4 wheels
//
//  The command arrays are ordered to match the `joints` lists in
//  config/controllers.yaml:  [FR, RR, FL, RL].
// ===========================================================================
#include <array>
#include <cmath>
#include <chrono>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>

using namespace std::chrono_literals;

namespace {
constexpr int    CONTROL_CYCLE = 10;     // [Hz]
constexpr double LENGTH = 0.75;          // chassis length
constexpr double WIDTH  = 0.75;          // chassis width
constexpr double HEIGHT = 0.25;          // chassis height
constexpr double RADIUS = 0.2;           // wheel radius

// 3-D Cartesian coordinate
struct Cartesian3 {
  double x;
  double y;
  double z;
};

// Wheel positions. Index order matches the controller joint order [FR,RR,FL,RL]
// (FR = +x/+y, RR = +x/-y, FL = -x/+y, RL = -x/-y), as defined in the URDF.
constexpr std::array<Cartesian3, 4> kWheel = {{
  { 0.5 * LENGTH,  0.5 * WIDTH, -0.5 * HEIGHT},  // n=0: FR
  { 0.5 * LENGTH, -0.5 * WIDTH, -0.5 * HEIGHT},  // n=1: RR
  {-0.5 * LENGTH,  0.5 * WIDTH, -0.5 * HEIGHT},  // n=2: FL
  {-0.5 * LENGTH, -0.5 * WIDTH, -0.5 * HEIGHT},  // n=3: RL
}};

// Swerve-drive inverse kinematics. Computes the steering angle (delta) and the
// wheel rotation speed (omega) for the commanded body twist (Ux, Uy, Uq).
void controllerKinematics(double delta[4], double omega[4],
                          double Ux, double Uy, double Uq)
{
  for (int n = 0; n < 4; ++n) {
    const double vx = Ux - kWheel[n].y * Uq;
    const double vy = Uy + kWheel[n].x * Uq;
    // previous steering angle
    const double delta_old = delta[n];
    // compute the steering angle
    if (vx * vx + vy * vy > 1.0E-5) {
      delta[n] = std::atan2(vy, vx);
    }
    // keep the steering change within +/-90 deg by flipping 180 deg
    while (delta[n] >= delta_old + M_PI / 2) delta[n] -= M_PI;
    while (delta[n] <= delta_old - M_PI / 2) delta[n] += M_PI;
    // compute the wheel rotation speed
    omega[n] = (vx * std::cos(delta[n]) + vy * std::sin(delta[n])) / RADIUS;
  }
}
}  // namespace

class ControllerKinematics : public rclcpp::Node {
public:
  ControllerKinematics() : rclcpp::Node("controller_kinematics")
  {
    sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", 1,
      std::bind(&ControllerKinematics::velocityCallback, this,
                std::placeholders::_1));

    pos_pub_ = create_publisher<std_msgs::msg::Float64MultiArray>(
      "/position_controller/commands", 1);
    vel_pub_ = create_publisher<std_msgs::msg::Float64MultiArray>(
      "/velocity_controller/commands", 1);

    timer_ = create_wall_timer(
      std::chrono::duration<double>(1.0 / CONTROL_CYCLE),
      std::bind(&ControllerKinematics::onTimer, this));

    RCLCPP_INFO(get_logger(),
                "controller_kinematics started (%d Hz)", CONTROL_CYCLE);
  }

private:
  void velocityCallback(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    velocity_in_[0] = msg->linear.x;
    velocity_in_[1] = msg->linear.y;
    velocity_in_[2] = msg->angular.z;
  }

  void onTimer()
  {
    controllerKinematics(delta_, omega_,
                         velocity_in_[0], velocity_in_[1], velocity_in_[2]);

    std_msgs::msg::Float64MultiArray pos_msg;
    std_msgs::msg::Float64MultiArray vel_msg;
    pos_msg.data.assign(delta_, delta_ + 4);
    vel_msg.data.assign(omega_, omega_ + 4);

    pos_pub_->publish(pos_msg);
    vel_pub_->publish(vel_msg);
  }

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
  rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr pos_pub_;
  rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr vel_pub_;
  rclcpp::TimerBase::SharedPtr timer_;

  double velocity_in_[3] = {0.0, 0.0, 0.0};
  double delta_[4] = {0.0, 0.0, 0.0, 0.0};  // target steering angle   [rad]
  double omega_[4] = {0.0, 0.0, 0.0, 0.0};  // target wheel speed      [rad/s]
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<ControllerKinematics>());
  rclcpp::shutdown();
  return 0;
}
