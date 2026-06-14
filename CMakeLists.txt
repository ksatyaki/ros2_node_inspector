cmake_minimum_required(VERSION 3.8)
project(ros2_node_inspector)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# --- Backend / diagnostics core (M1). GUI deps (GLFW/ImGui) arrive in M2. ---
add_library(node_inspector_core
  src/qos_compat.cpp
  src/status.cpp
  src/graph_model.cpp
  src/liveness_probe.cpp
)
target_include_directories(node_inspector_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
ament_target_dependencies(node_inspector_core rclcpp)

# --- Manual harness CLI (graph_model + liveness against a live graph). ---
add_executable(inspect_cli src/inspect_cli.cpp)
target_link_libraries(inspect_cli node_inspector_core)
ament_target_dependencies(inspect_cli rclcpp)

install(TARGETS node_inspector_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS inspect_cli
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_qos_compat test/test_qos_compat.cpp)
  target_link_libraries(test_qos_compat node_inspector_core)
  ament_target_dependencies(test_qos_compat rclcpp)

  ament_add_gtest(test_status test/test_status.cpp)
  target_link_libraries(test_status node_inspector_core)
  ament_target_dependencies(test_status rclcpp)

  ament_add_gtest(test_counts test/test_counts.cpp)
  target_link_libraries(test_counts node_inspector_core)
  ament_target_dependencies(test_counts rclcpp)

  ament_add_gtest(test_rate_window test/test_rate_window.cpp)
  target_link_libraries(test_rate_window node_inspector_core)
  ament_target_dependencies(test_rate_window rclcpp)
endif()

ament_package()
