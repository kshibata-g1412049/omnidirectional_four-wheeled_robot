// ===========================================================================
//  odometry_publisher.cpp
//  author: Koji Shibata
//  e-mail: kshibata.0519@gmail.com
//
//  Forward-kinematics odometry for the omnidirectional four-wheeled
//  (swerve-drive) robot. Answers GitHub issue #1 ("how to calculate the
//  odom of a four-wheeled omnidirectional robot?").
//
//  Subscribes : /joint_states  (sensor_msgs/msg/JointState)
//  Publishes  : /odom          (nav_msgs/msg/Odometry)
//               tf "odom" -> "base_link"
//
//  Forward kinematics derivation
//  ------------------------------
//  Each wheel n reports a measured contact-point velocity in the body frame:
//    vxm[n] = RADIUS * omega[n] * cos(delta[n])
//    vym[n] = RADIUS * omega[n] * sin(delta[n])
//  where delta[n] is the *_joint1 (steering) position and omega[n] is the
//  *_joint2 (drive) velocity, read from /joint_states.
//
//  The inverse problem (controller_kinematics.cpp) relates body twist
//  (Ux, Uy, Uq) to per-wheel velocity by:
//    vx[n] = Ux - wheel[n].y * Uq
//    vy[n] = Uy + wheel[n].x * Uq
//  i.e. a linear system A*q = b with 8 rows (vx/vy per wheel) and 3 unknowns
//  q = [Ux, Uy, Uq]. Because this robot's wheels are placed symmetrically at
//  (+-x, +-y) about the chassis center, sum(x)=sum(y)=sum(x*y)=0, which makes
//  the least-squares normal-equation matrix A^T*A exactly diagonal. The
//  least-squares solution therefore reduces to simple closed-form averages
//  below -- no matrix/Eigen dependency needed, and the result is identical
//  to a full pseudo-inverse solve for this wheel layout.
// ===========================================================================
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>

#include "omnidirectional_four_wheeled_robot/robot_geometry.hpp"

using omnidirectional_four_wheeled_robot::kWheel;
using omnidirectional_four_wheeled_robot::kWheelPrefix;
using omnidirectional_four_wheeled_robot::RADIUS;

namespace {

// Sum_n(x[n]^2 + y[n]^2), computed once from kWheel rather than hardcoded, so
// this stays correct if the chassis dimensions ever change.
double yawDenominator()
{
  double sum = 0.0;
  for (const auto & w : kWheel) {
    sum += w.x * w.x + w.y * w.y;
  }
  return sum;
}

// Forward kinematics: recovers the body twist (Ux, Uy, Uq) that best
// explains the 4 wheels' measured steering angles and drive speeds.
void forwardKinematics(const std::array<double, 4> & delta,
                        const std::array<double, 4> & omega,
                        double & Ux, double & Uy, double & Uq)
{
  static const double kYawDenom = yawDenominator();

  double sum_vx = 0.0;
  double sum_vy = 0.0;
  double sum_yaw = 0.0;
  for (int n = 0; n < 4; ++n) {
    const double vxm = RADIUS * omega[n] * std::cos(delta[n]);
    const double vym = RADIUS * omega[n] * std::sin(delta[n]);
    sum_vx += vxm;
    sum_vy += vym;
    sum_yaw += kWheel[n].x * vym - kWheel[n].y * vxm;
  }
  Ux = sum_vx / 4.0;
  Uy = sum_vy / 4.0;
  Uq = sum_yaw / kYawDenom;
}

}  // namespace

class OdometryPublisher : public rclcpp::Node {
public:
  OdometryPublisher() : rclcpp::Node("odometry_publisher")
  {
    sub_ = create_subscription<sensor_msgs::msg::JointState>(
      "/joint_states", 1,
      std::bind(&OdometryPublisher::jointStateCallback, this,
                std::placeholders::_1));

    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("/odom", 10);
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    RCLCPP_INFO(get_logger(), "odometry_publisher started");
  }

private:
  void jointStateCallback(const sensor_msgs::msg::JointState::SharedPtr msg)
  {
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < msg->name.size(); ++i) {
      index[msg->name[i]] = i;
    }

    std::array<double, 4> delta{};
    std::array<double, 4> omega{};
    for (int n = 0; n < 4; ++n) {
      const std::string steer_joint = std::string(kWheelPrefix[n]) + "wheel_joint1";
      const std::string drive_joint = std::string(kWheelPrefix[n]) + "wheel_joint2";
      const auto steer_it = index.find(steer_joint);
      const auto drive_it = index.find(drive_joint);
      if (steer_it == index.end() || drive_it == index.end() ||
          steer_it->second >= msg->position.size() ||
          drive_it->second >= msg->velocity.size())
      {
        if (warn_count_ < 5) {
          RCLCPP_WARN(get_logger(),
                      "missing joint state for %s/%s, skipping this update",
                      steer_joint.c_str(), drive_joint.c_str());
          ++warn_count_;
        }
        return;
      }
      delta[n] = msg->position[steer_it->second];
      omega[n] = msg->velocity[drive_it->second];
    }

    double Ux = 0.0, Uy = 0.0, Uq = 0.0;
    forwardKinematics(delta, omega, Ux, Uy, Uq);

    const rclcpp::Time stamp(msg->header.stamp);
    if (last_stamp_.nanoseconds() == 0) {
      last_stamp_ = stamp;
      return;
    }
    const double dt = (stamp - last_stamp_).seconds();
    last_stamp_ = stamp;
    if (dt <= 0.0 || dt > 1.0) {
      // First sample, a clock jump, or a stale/duplicate message -- skip
      // integrating this step rather than corrupting the pose estimate.
      return;
    }

    // 2nd-order midpoint pose integration (same approach as
    // ros2_controllers' diff_drive_controller), extended from a 1-D forward
    // speed to a full holonomic body twist.
    const double delta_theta = Uq * dt;
    const double theta_mid = theta_ + 0.5 * delta_theta;
    x_ += (Ux * std::cos(theta_mid) - Uy * std::sin(theta_mid)) * dt;
    y_ += (Ux * std::sin(theta_mid) + Uy * std::cos(theta_mid)) * dt;
    theta_ = std::atan2(std::sin(theta_ + delta_theta), std::cos(theta_ + delta_theta));

    // Yaw-only rotation -> quaternion, computed directly rather than via
    // tf2::Quaternion::setRPY(): the tf2 LinearMath headers needed for that
    // are not available on every distro (e.g. removed on Rolling), and a
    // pure-yaw quaternion is simple enough not to need the helper anyway.
    geometry_msgs::msg::Quaternion orientation;
    orientation.z = std::sin(0.5 * theta_);
    orientation.w = std::cos(0.5 * theta_);

    nav_msgs::msg::Odometry odom_msg;
    odom_msg.header.stamp = msg->header.stamp;
    odom_msg.header.frame_id = "odom";
    odom_msg.child_frame_id = "base_link";
    odom_msg.pose.pose.position.x = x_;
    odom_msg.pose.pose.position.y = y_;
    odom_msg.pose.pose.position.z = 0.0;
    odom_msg.pose.pose.orientation = orientation;
    // Placeholder covariance: small but nonzero, since the diagonal-only
    // wheel layout assumption and unmodeled slip mean this is not exact.
    // Never report exact zero -- downstream consumers (e.g. an EKF) would
    // read that as "infinitely confident".
    odom_msg.pose.covariance[0] = 1.0e-2;   // x
    odom_msg.pose.covariance[7] = 1.0e-2;   // y
    odom_msg.pose.covariance[35] = 5.0e-2;  // yaw
    odom_msg.twist.twist.linear.x = Ux;
    odom_msg.twist.twist.linear.y = Uy;
    odom_msg.twist.twist.angular.z = Uq;
    odom_msg.twist.covariance[0] = 1.0e-2;
    odom_msg.twist.covariance[7] = 1.0e-2;
    odom_msg.twist.covariance[35] = 5.0e-2;
    odom_pub_->publish(odom_msg);

    geometry_msgs::msg::TransformStamped tf_msg;
    tf_msg.header.stamp = msg->header.stamp;
    tf_msg.header.frame_id = "odom";
    tf_msg.child_frame_id = "base_link";
    tf_msg.transform.translation.x = x_;
    tf_msg.transform.translation.y = y_;
    tf_msg.transform.translation.z = 0.0;
    tf_msg.transform.rotation = orientation;
    tf_broadcaster_->sendTransform(tf_msg);
  }

  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  rclcpp::Time last_stamp_{0, 0, RCL_ROS_TIME};
  double x_ = 0.0;
  double y_ = 0.0;
  double theta_ = 0.0;
  int warn_count_ = 0;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<OdometryPublisher>());
  rclcpp::shutdown();
  return 0;
}
