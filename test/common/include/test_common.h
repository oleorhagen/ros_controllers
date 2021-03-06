///////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2013, PAL Robotics S.L.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   * Neither the name of PAL Robotics, Inc. nor the names of its
//     contributors may be used to endorse or promote products derived from
//     this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//////////////////////////////////////////////////////////////////////////////

/// \author Bence Magyar
/// \author Masaru Morita

#pragma once

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>

#include <gtest/gtest.h>

#include <ros/ros.h>

#include <geometry_msgs/Twist.h>
#include <nav_msgs/Odometry.h>
#include <tf/tf.h>

#include <controller_manager_msgs/ListControllers.h>
#include <std_srvs/Empty.h>

// Floating-point value comparison threshold
const double EPS = 0.01;
const double POSITION_TOLERANCE = 0.01;               // 1 cm-s precision
const double VELOCITY_TOLERANCE = 0.02;               // 2 cm-s-1 precision
const double ANGULAR_VELOCITY_TOLERANCE = 0.05;       // 3 deg-s-1 precision
const double JERK_LINEAR_VELOCITY_TOLERANCE = 0.10;   // 10 cm-s-1 precision
const double JERK_ANGULAR_VELOCITY_TOLERANCE = 0.05;  // 3 deg-s-1 precision
const double ORIENTATION_TOLERANCE = 0.03;            // 0.57 degree precision

class AckermannSteeringControllerTest : public ::testing::Test {
 public:
  AckermannSteeringControllerTest()
      : cmd_pub(nh.advertise<geometry_msgs::Twist>("cmd_vel", 100)),
        odom_sub(nh.subscribe(
            "odom", 100, &AckermannSteeringControllerTest::odomCallback, this)),
        start_srv(nh.serviceClient<std_srvs::Empty>("start")),
        stop_srv(nh.serviceClient<std_srvs::Empty>("stop")),
        list_ctrls_srv(
            nh.serviceClient<controller_manager_msgs::ListControllers>(
                "/controller_manager/list_controllers")),
        ctrl_name("ackermann_steering_bot_controller") {}

  ~AckermannSteeringControllerTest() { odom_sub.shutdown(); }

  nav_msgs::Odometry getLastOdom() {
    std::lock_guard<std::mutex> lock(odom_mutex);
    return last_odom;
  }

  bool isLastOdomValid() {
    try {
      auto odom = getLastOdom();
      tf::assertQuaternionValid(odom.pose.pose.orientation);
    } catch (const tf::InvalidArgument& exception) {
      return false;
    }
    return true;
  }

  void publish(geometry_msgs::Twist cmd_vel) { cmd_pub.publish(cmd_vel); }

  bool isControllerAlive() {
    controller_manager_msgs::ListControllers srv;
    list_ctrls_srv.call(srv);

    auto ctrl_list = srv.response.controller;
    auto is_running =
        [this](const controller_manager_msgs::ControllerState& ctrl) {
          return ctrl.name == ctrl_name && ctrl.state == "running";
        };
    bool running = std::any_of(ctrl_list.begin(), ctrl_list.end(), is_running);
    bool subscribing = cmd_pub.getNumSubscribers() > 0;
    return running && subscribing;
  }

  void start() {
    std_srvs::Empty srv;
    start_srv.call(srv);
  }
  void stop() {
    std_srvs::Empty srv;
    stop_srv.call(srv);
  }

 private:
  ros::NodeHandle nh;
  ros::Publisher cmd_pub;
  ros::Subscriber odom_sub;
  nav_msgs::Odometry last_odom;

  ros::ServiceClient start_srv;
  ros::ServiceClient stop_srv;

  ros::ServiceClient list_ctrls_srv;
  std::string ctrl_name;

  std::mutex odom_mutex;

  void odomCallback(const nav_msgs::Odometry& odom) {
    ROS_INFO_STREAM("Callback reveived: pos.x: "
                    << odom.pose.pose.position.x
                    << ", orient.z: " << odom.pose.pose.orientation.z
                    << ", lin_est: " << odom.twist.twist.linear.x
                    << ", ang_est: " << odom.twist.twist.angular.z);
    std::lock_guard<std::mutex> lock(odom_mutex);
    last_odom = odom;
  }
};

inline tf::Quaternion tfQuatFromGeomQuat(
    const geometry_msgs::Quaternion& quat) {
  return tf::Quaternion(quat.x, quat.y, quat.z, quat.w);
}
