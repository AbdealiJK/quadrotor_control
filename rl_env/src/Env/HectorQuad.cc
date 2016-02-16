#include <rl_env/HectorQuad.hh>

HectorQuad::HectorQuad()
{
  n_action = 4;
  n_state = 8;
  n_policy = 8;

  s.resize(n_state);
  phy_steps = 10;
  cur_step = 0;

  // Set name of model
  initial.model_name = "quadrotor";
  final.model_name = "quadrotor";
  current.model_name = "quadrotor";

  // initial.model_name = "";
  // final.model_name = "";
  // current.model_name = "";
  
  ros::NodeHandle node;

  // Publishers
  // cmd_vel = node.advertise<geometry_msgs::Twist>("/cmd_vel", 5);
  command_twist = node.advertise<geometry_msgs::TwistStamped>("/command/twist", 5);
  // motor_pwm = node.advertise<hector_uav_msgs::MotorPWM>("/motor_pwm", 5);

  // Services
  ros::service::waitForService("/gazebo/reset_world", -1);
  reset_world = node.serviceClient<std_srvs::Empty>("/gazebo/reset_world");
  ros::service::waitForService("/rl_env/run_sim", -1);
  run_sim = node.serviceClient<rl_common::RLRunSim>("/rl_env/run_sim");
  ros::service::waitForService("/gazebo/pause_physics", -1);
  pause_phy = node.serviceClient<std_srvs::Empty>("/gazebo/pause_physics");
  ros::service::waitForService("/gazebo/set_model_state", -1);
  set_model_state =
    node.serviceClient<gazebo_msgs::SetModelState>("/gazebo/set_model_state");

  // Load the controller needed. If this it done in launch file, it doesnt
  // start on time. So, it needs to be done in sync.
  ros::service::waitForService("/controller_manager/load_controller", -1);
  load_controller =
    node.serviceClient<controller_manager_msgs::LoadController>(
      "/controller_manager/load_controller");
  controller_manager_msgs::LoadController load_msg;
  load_msg.request.name = "controller/twist";
  load_controller.call(load_msg);
  assert(load_msg.response.ok);

  // Check list of controllers loaded
  ros::service::waitForService("/controller_manager/list_controllers", -1);
  list_controllers =
    node.serviceClient<controller_manager_msgs::ListControllers>(
      "/controller_manager/list_controllers");
  controller_manager_msgs::ListControllers list_msg;
  list_controllers.call(list_msg);
  // The assert assumes that the first controller is twist.
  assert(list_msg.response.controller[0].name == load_msg.request.name);

  ros::service::waitForService("/engage", -1);
  engage = node.serviceClient<std_srvs::Empty>("/engage");
  ros::service::waitForService("/shutdown", -1);
  shutdown = node.serviceClient<std_srvs::Empty>("/shutdown");

  reset();
  for ( double i = 0; i < 4; i+=0.1 ) {
    double r=0, p=0, y=i, ny;
    tf::Matrix3x3 m = tf::Matrix3x3();
    m.setRPY(r, p, y);
    m.getRPY(r, p, ny);
    std::cout << y << "  -  " << ny << "\n";
  }
}

const std::vector<float> &HectorQuad::sensation() {
  // Get state from gazebo and save to "current" state
  rl_common::RLRunSim msg;
  msg.request.steps = phy_steps;
  cur_step += phy_steps;
  run_sim.call(msg);
  current.pose = msg.response.pose;
  current.twist = msg.response.twist;

  get_trajectory();

  // Convert gazebo's state to internal representation
  s[0] = final.pose.position.z - current.pose.position.z;
  s[1] = current.twist.linear.z;
  s[2] = final.pose.position.y - current.pose.position.y;
  s[3] = current.twist.linear.y;
  s[4] = final.pose.position.x - current.pose.position.x;
  s[5] = current.twist.linear.x;
  s[6] = tf::getYaw(final.pose.orientation) - tf::getYaw(current.pose.orientation);
  s[7] = current.twist.angular.z;
  // std::cout << s << std::endl;
  return s;
}

bool HectorQuad::terminal() {
  if (cur_step > 10000000) return true;
  return false;
}

float HectorQuad::apply(std::vector<float> action) {
  // The below assert is an "in case" - to check whether the controller
  // is actually engaged before giving the action.
  controller_manager_msgs::ListControllers list_msg;
  list_controllers.call(list_msg);
  assert(list_msg.response.controller[0].state == "running");

  // Send action
  assert(action.size() == n_action);
  geometry_msgs::TwistStamped action_vel;
  action_vel.twist.linear.z = action[0];
  action_vel.twist.linear.y = action[1];
  action_vel.twist.linear.x = action[2];
  action_vel.twist.angular.z = action[3];
  command_twist.publish(action_vel);
  return reward();
}

float HectorQuad::reward() {
  tf::Quaternion curr_quat;
  double curr_roll, curr_pitch, curr_yaw;
  tf::quaternionMsgToTF(current.pose.orientation, curr_quat);
  tf::Matrix3x3(curr_quat).getRPY(curr_roll, curr_pitch, curr_yaw);

  tf::Quaternion final_quat;
  double final_roll, final_pitch, final_yaw;
  tf::quaternionMsgToTF(final.pose.orientation, final_quat);
  tf::Matrix3x3(final_quat).getRPY(final_roll, final_pitch, final_yaw);

  // std::cout << "Yaw : " << curr_yaw << " - " << final_yaw << "\n";

  return (
    -fabs(final.pose.position.z - current.pose.position.z)
    -fabs(final.pose.position.y - current.pose.position.y)
    -fabs(final.pose.position.x - current.pose.position.x)
    // -fabs(final_roll - final_roll) * 10.0
    // -fabs(final_pitch - final_pitch) * 10.0
    -fabs(curr_yaw - final_yaw) * 10.0
    // -fabs(pitch) * 10.0
    // -fabs(roll) * 10.0
  );
}

void HectorQuad::reset() {
  shutdown.call(empty_msg); // shutdown motors
  geometry_msgs::TwistStamped action_vel; // Set velocity to 0

  // Keep checking the status of controller. When it goes into "running"
  // we can start getting actions from the agent.
  // Until then, keep giving a vel of 0 so that it will auto engage.
  // std::cout << "HectorQuad : Waiting for controller to engage motors ...\n";
  while(1) {
    controller_manager_msgs::ListControllers list_msg;
    list_controllers.call(list_msg);
    command_twist.publish(action_vel);

    usleep(50);
    if (list_msg.response.controller[0].state == "running")
      break;
  }

  initial.pose.position.x = 0;
  initial.pose.position.y = 0;
  initial.pose.position.z = 0;
  initial.pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(0, 0, 0);

  initial.twist.linear.x = 0;
  initial.twist.linear.y = 0;
  initial.twist.linear.z = 0;
  initial.twist.angular.x = 0;
  initial.twist.angular.y = 0;
  initial.twist.angular.z = 0;

  // Reset and pause the world
  // Note: Pause has to be done only after `waitForService` finds the service.
  //       it cannot be done in gazebo as otherwise waitForService hangs.
  pause_phy.call(empty_msg);
  reset_world.call(empty_msg);
  cur_step = 0;

  // set initial position programmatically
  gazebo_msgs::SetModelState msg;
  msg.request.model_state = initial;
  assert(set_model_state.call(msg));
}

void HectorQuad::get_trajectory(long long time_in_steps /* = -1 */) {
  if (time_in_steps == -1) time_in_steps = cur_step;

  final.pose.position.x = 0;
  final.pose.position.y = 0;
  final.pose.position.z = 5;
  final.pose.orientation = tf::createQuaternionMsgFromRollPitchYaw(
    0, 0, angles::from_degrees(10));
  if ( time_in_steps < 5000 ) {
  } else {
    final.pose.position.x = 5 * sin(0.0001 * (time_in_steps - 5000));
    final.pose.position.y = 5 * cos(0.0001 * (time_in_steps - 5000));
    final.pose.position.z = 5;
  }

  final.twist.linear.x = 0;
  final.twist.linear.y = 0;
  final.twist.linear.z = 0;
  final.twist.angular.x = 0;
  final.twist.angular.y = 0;
  final.twist.angular.z = 0;
}
