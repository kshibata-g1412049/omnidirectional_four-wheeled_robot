// ===========================================================================
//  robot_geometry.hpp
//
//  Wheel geometry shared between the inverse-kinematics controller
//  (controller_kinematics.cpp) and the odometry publisher
//  (odometry_publisher.cpp). Both nodes must agree on the exact same wheel
//  positions and radius, or their kinematics will silently diverge -- keep
//  these constants in one place rather than duplicating them per file.
// ===========================================================================
#ifndef OMNIDIRECTIONAL_FOUR_WHEELED_ROBOT__ROBOT_GEOMETRY_HPP_
#define OMNIDIRECTIONAL_FOUR_WHEELED_ROBOT__ROBOT_GEOMETRY_HPP_

#include <array>

namespace omnidirectional_four_wheeled_robot {

constexpr double LENGTH = 0.75;  // chassis length
constexpr double WIDTH  = 0.75;  // chassis width
constexpr double RADIUS = 0.2;   // wheel radius

// 3-D Cartesian coordinate
struct Cartesian3 {
  double x;
  double y;
  double z;
};

// Wheel positions relative to the chassis center. Index order is
// [FR, RR, FL, RL], matching the `joints` lists in config/controllers.yaml
// and the `name` prefixes below.
constexpr std::array<Cartesian3, 4> kWheel = {{
  { 0.5 * LENGTH,  0.5 * WIDTH, 0.0},  // n=0: FR
  { 0.5 * LENGTH, -0.5 * WIDTH, 0.0},  // n=1: RR
  {-0.5 * LENGTH,  0.5 * WIDTH, 0.0},  // n=2: FL
  {-0.5 * LENGTH, -0.5 * WIDTH, 0.0},  // n=3: RL
}};

// Joint-name prefixes, same index order as kWheel. Joint names follow the
// pattern "{prefix}wheel_joint1" (steering) / "{prefix}wheel_joint2" (drive).
constexpr std::array<const char *, 4> kWheelPrefix = {"FR", "RR", "FL", "RL"};

}  // namespace omnidirectional_four_wheeled_robot

#endif  // OMNIDIRECTIONAL_FOUR_WHEELED_ROBOT__ROBOT_GEOMETRY_HPP_
