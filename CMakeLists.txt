cmake_minimum_required(VERSION 2.8.3)
project(bookstore_robot_nav)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  std_msgs
  geometry_msgs
  sensor_msgs
  nav_msgs
  actionlib
  actionlib_msgs
  move_base_msgs
  tf
  cv_bridge
)

find_package(OpenCV REQUIRED)

# Find yaml-cpp using pkg-config or standard find_package
find_package(yaml-cpp QUIET)
if(NOT yaml-cpp_FOUND)
  # Fallback: try to find it manually
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(YAML_CPP yaml-cpp)
endif()

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES
  CATKIN_DEPENDS roscpp rospy std_msgs geometry_msgs sensor_msgs nav_msgs actionlib actionlib_msgs move_base_msgs tf cv_bridge
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${YAML_CPP_INCLUDE_DIRS}
)

# Task Manager Node
add_executable(task_manager src/task_manager.cpp)
target_link_libraries(task_manager 
  ${catkin_LIBRARIES} 
  ${OpenCV_LIBRARIES} 
  ${YAML_CPP_LIBRARIES}
)
add_dependencies(task_manager ${catkin_EXPORTED_TARGETS})

# QR Reader Node
add_executable(qr_reader src/qr_reader.cpp)
target_link_libraries(qr_reader 
  ${catkin_LIBRARIES} 
  zbar
  ${OpenCV_LIBRARIES}
)
add_dependencies(qr_reader ${catkin_EXPORTED_TARGETS})
