// ===========================================================================
//  spawner_sphere.cpp
//  author: Koji Shibata  (ROS 2 port)
//  e-mail: kshibata.0519@gmail.com
//
//  Spawns a sphere into the running Gazebo (gz) simulation by calling the
//  `/world/<world>/create` service provided by ros_gz_sim.
//
//  The model file is resolved from this package's installed share directory
//  (urdf/sphere.urdf) via ament_index, instead of a hard-coded absolute path.
//
//  Parameters:
//    world   (string)  : gz world name           (default: "empty")
//    x, y, z (double)  : initial spawn position   (default: 3, 3, 3)
// ===========================================================================
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>
#include <ros_gz_interfaces/srv/spawn_entity.hpp>

using namespace std::chrono_literals;

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = rclcpp::Node::make_shared("spawner_sphere");

  const std::string world = node->declare_parameter<std::string>("world", "empty");
  const double x = node->declare_parameter<double>("x", 3.0);
  const double y = node->declare_parameter<double>("y", 3.0);
  const double z = node->declare_parameter<double>("z", 3.0);

  // Resolve the sphere description from the package share directory.
  const std::string share =
    ament_index_cpp::get_package_share_directory("omnidirectional_four_wheeled_robot");
  const std::string urdf_path = share + "/urdf/sphere.urdf";

  std::ifstream ifs(urdf_path);
  if (!ifs) {
    RCLCPP_ERROR(node->get_logger(), "Cannot open model file: %s", urdf_path.c_str());
    rclcpp::shutdown();
    return 1;
  }
  std::stringstream ss;
  ss << ifs.rdbuf();
  const std::string model_xml = ss.str();

  const std::string service = "/world/" + world + "/create";
  auto client = node->create_client<ros_gz_interfaces::srv::SpawnEntity>(service);

  RCLCPP_INFO(node->get_logger(), "Waiting for spawn service: %s", service.c_str());
  if (!client->wait_for_service(10s)) {
    RCLCPP_ERROR(node->get_logger(), "Spawn service not available: %s", service.c_str());
    rclcpp::shutdown();
    return 1;
  }

  auto request = std::make_shared<ros_gz_interfaces::srv::SpawnEntity::Request>();
  request->entity_factory.name = "sphere";
  request->entity_factory.allow_renaming = true;
  request->entity_factory.sdf = model_xml;
  request->entity_factory.pose.position.x = x;
  request->entity_factory.pose.position.y = y;
  request->entity_factory.pose.position.z = z;
  request->entity_factory.pose.orientation.w = 1.0;

  auto future = client->async_send_request(request);
  if (rclcpp::spin_until_future_complete(node, future) ==
      rclcpp::FutureReturnCode::SUCCESS)
  {
    RCLCPP_INFO(node->get_logger(), "Sphere spawned (success=%s)",
                future.get()->success ? "true" : "false");
  } else {
    RCLCPP_ERROR(node->get_logger(), "Failed to call spawn service");
  }

  rclcpp::shutdown();
  return 0;
}
