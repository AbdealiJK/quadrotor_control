#include <geometry_msgs/Twist.h>
#include <rl_env/HectorQuad.hh>
#include <std_srvs/Empty.h>

// Random initialization of position
HectorQuad::HectorQuad(Random &rand, Eigen::Vector3d target/* = Eigen::Vector3d(0, 0, 0)*/):
  s(2),
  rng(rand),
  target_pos(target),
  num_actions(3),
  pos(0, 0, s[0]),
  vel(0, 0, s[1]),
  last_pos(0, 0, s[0]),
  last_vel(0, 0, s[1])
{
  ros::NodeHandle node;
  int qDepth = 1;
  ros::TransportHints noDelay = ros::TransportHints().tcpNoDelay(true);
  // Publishers
  cmd_vel = node.advertise<geometry_msgs::Twist>("/cmd_vel", 5);
  // Subscribers
  ground_truth = node.subscribe<nav_msgs::Odometry>("/ground_truth/state", qDepth, &HectorQuad::gazeboGroundTruth, this, noDelay);
  // Services
  reset_world = node.serviceClient<std_srvs::Empty>("/gazebo/reset_world");
  reset();
}

HectorQuad::~HectorQuad() { }

void HectorQuad::gazeboGroundTruth(const nav_msgs::Odometry::ConstPtr& msg) {
  last_pos = pos;
  last_vel = vel;
  pos(2) = msg->pose.pose.position.z;
  vel(2) = msg->twist.twist.linear.z;
  // std::cout << "Pos: " << pos(2) << " Vel: " << vel(2) << std::endl;
}

void HectorQuad::refreshState() {
  s[0] = (pos(2)-target_pos(2) >= 0)?1:0;
  s[1] = (pos(2)-target_pos(2) <= 0)?1:0;
}

const std::vector<float> &HectorQuad::sensation() {
  refreshState();
  return s;
}

int HectorQuad::getNumActions() {
  return num_actions;
}

bool HectorQuad::terminal() {
  if (abs(target_pos(2) - pos(2)) < 0.2) {
    terminal_count += 1;
    if (terminal_count >= 1000) {
      return(true);
    }
  } else {
    terminal_count = 0;
  }
  return(false);
}

// Called by env.cpp for next action
float HectorQuad::apply(int action) {
  geometry_msgs::Twist action_vel;
  switch(action) {
    case UP:
      action_vel.linear.z = +2;
      break;
    case DOWN:
      action_vel.linear.z = -2;
      break;
    case STAY:
      action_vel.linear.z = 0;
      break;
  }
  // std::cout << "Action:" << action_vel.linear.z << " State:" << s[0] << ","
  //           << s[1] << " Pos:" << pos(2) << " Targ:" << target_pos(2)
  //           << " Reward:" << reward() << std::endl;

  cmd_vel.publish(action_vel);

  return reward();
}

// Reward policy function
float HectorQuad::reward() {
  if ( abs(target_pos(2) - pos(2)) < abs(target_pos(2) - last_pos(2)) ) {
    return 1;
  } else if ( abs(target_pos(2) - pos(2)) > abs(target_pos(2) - last_pos(2)) ) {
    return -1;
  }
}

void HectorQuad::setSensation(std::vector<float> newS){
  if (s.size() != newS.size()){
    std::cerr << "Error in sensation sizes" << std::endl;
  }

  for (unsigned i = 0; i < newS.size(); i++){
    s[i] = newS[i];
  }
}

std::vector<experience> HectorQuad::getSeedings() {
  // return seedings
  std::vector<experience> seeds;
  reset();

  return seeds;
}

experience HectorQuad::getExp(float s0, float s1, int a){
  experience e;
  e.s.resize(2, 0.0);
  e.next.resize(2, 0.0);

  pos(2) = s0;
  vel(2) = s1;

  e.act = a;
  e.s = sensation();
  e.reward = apply(e.act);

  e.terminal = terminal();
  e.next = sensation();

  return e;
}

void HectorQuad::getMinMaxFeatures(std::vector<float> *minFeat, std::vector<float> *maxFeat) {
  // No clue what model to impleent here
}


void HectorQuad::getMinMaxReward(float* minR, float* maxR) {
  *minR = -(num_actions-1)/2;
  *maxR = (num_actions-1)/2;
}

void HectorQuad::reset() {
  terminal_count = 0;
  // Reset the world
  std_srvs::Empty msg;
  reset_world.call(msg);
}
