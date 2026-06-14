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
find_package(ament_index_cpp REQUIRED)
find_package(glfw3 REQUIRED)
set(OpenGL_GL_PREFERENCE GLVND)  # silence FindOpenGL CMP0072 legacy/GLVND warning
find_package(OpenGL REQUIRED)

# --- Backend / diagnostics core (M1). Pure logic + rclcpp graph reads. ---
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

# --- Vendored Dear ImGui (core + GLFW/OpenGL3 backends), built as a static lib.
#     Compiled with -w: third-party headers are not held to our warning flags. --
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
target_compile_options(imgui PRIVATE -w)

# --- GUI application (M2): renders the M1 model via ImGui. ---
add_executable(node_inspector
  src/main.cpp
  src/icons.cpp
  src/view_table.cpp
  src/view_graph.cpp
)
target_link_libraries(node_inspector node_inspector_core imgui glfw OpenGL::GL)
ament_target_dependencies(node_inspector rclcpp ament_index_cpp)

install(TARGETS node_inspector_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS inspect_cli node_inspector
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

# Demo graph (parametrized rclpy talker/listener + launch file) for testing the
# GUI. Installed so `ros2 launch ros2_node_inspector qos_demo.launch.py` works;
# also runnable straight from source.
install(DIRECTORY demo
  DESTINATION share/${PROJECT_NAME}
  USE_SOURCE_PERMISSIONS
)

# Optional FontAwesome glyphs are looked up at runtime in the package share dir.
# The .ttf is git-ignored (user-supplied per the font protocol); install it only
# when present so the build never hard-depends on it.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/fa-solid-900.ttf)
  install(FILES third_party/fonts/fa-solid-900.ttf
    DESTINATION share/${PROJECT_NAME}/fonts)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/PT_Sans-Web-Regular.ttf)
  install(FILES third_party/fonts/PT_Sans-Web-Regular.ttf
    DESTINATION share/${PROJECT_NAME}/fonts)
endif()
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/PT_Sans-Web-Bold.ttf)
  install(FILES third_party/fonts/PT_Sans-Web-Bold.ttf
    DESTINATION share/${PROJECT_NAME}/fonts)
endif()

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
