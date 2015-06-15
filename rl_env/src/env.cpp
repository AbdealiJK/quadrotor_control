#include <ros/ros.h>
#include "std_msgs/String.h"

#include <rl_msgs/RLStateReward.h>
#include <rl_msgs/RLEnvDescription.h>
#include <rl_msgs/RLAction.h>
#include <rl_msgs/RLExperimentInfo.h>
#include <rl_msgs/RLEnvSeedExperience.h>

#include <ros/callback_queue.h>
#include <tf/transform_broadcaster.h>

#include <rl_common/core.hh>
#include <rl_common/Random.h>

#include <rl_env/TwoRooms.hh>
#include <rl_env/Taxi.hh>
#include <rl_env/MountainCar.hh>
#include <rl_env/HectorQuad.hh>

#include <getopt.h>
#include <stdlib.h>

#define NODE "RLEnvironment"

static ros::Publisher out_env_desc;
static ros::Publisher out_env_sr;
static ros::Publisher out_seed;

Environment* e;
Random rng;
bool PRINTS = 0;//true;
int seed = 1;
char* envType;

// some default parameters
bool stochastic = true;
int nstocks = 3;
int nsectors = 3;
int delay = 0;
bool lag = false;
bool highvar = false;


void displayHelp(){
  std::cout << std::endl << " Call env --env type [options]" << std::endl;
  std::cout << "Env types: taxi tworooms fourrooms energy fuelworld mcar "
               "cartpole car2to7 car7to2 carrandom stocks lightworld hectorquad" << std::endl;
  std::cout << std::endl << " Options:" << std::endl;
  std::cout << "--seed value (integer seed for random number generator)"
            << std::endl;
  std::cout << "--deterministic (deterministic version of domain)" << std::endl;
  std::cout << "--stochastic (stochastic version of domain)" << std::endl;
  std::cout << "--delay value (# steps of action delay (for mcar and "
            << "tworooms)" << std::endl;
  std::cout << "--prints (turn on debug printing of actions/rewards)"
            << std::endl;
  exit(-1);
}


void processAction(const rl_msgs::RLAction::ConstPtr &actionIn){
  /** process action from the agent */
  rl_msgs::RLStateReward sr;

  // process action from the agent, affecting the environment
  sr.reward = e->apply(actionIn->action);
  sr.state = e->sensation();
  sr.terminal = e->terminal();

  // publish the state-reward message
  if (PRINTS >= 1)
    std::cout << "Got action " << actionIn->action << " at state: "
              << sr.state[0] << ", " << sr.state[1] << ", reward: "
              << sr.reward << std::endl;

  out_env_sr.publish(sr);

}

void processEpisodeInfo(const rl_msgs::RLExperimentInfo::ConstPtr &infoIn){
  /** Process end-of-episode reward info. Mostly to start new episode. */
  if (PRINTS >= 2)
    std::cout << "Episode " << infoIn->episode_number <<" terminated with "
              << "reward: " << infoIn->episode_reward << ", start new episode "
              << std::endl;

  e->reset();

  rl_msgs::RLStateReward sr;
  sr.reward = 0;
  sr.state = e->sensation();
  sr.terminal = false;
  out_env_sr.publish(sr);
}

void initEnvironment(){
  /** init the environment, publish a description. */
  // init the environment
  e = NULL;
  rl_msgs::RLEnvDescription desc;

  if (strcmp(envType, "mcar") == 0){
    desc.title = "Environment: Mountain Car\n";
    e = new MountainCar(rng, stochastic, false, delay);
  } else if (strcmp(envType, "taxi") == 0){
    desc.title = "Environment: Taxi\n";
    e = new Taxi(rng, stochastic);
  } else if (strcmp(envType, "tworooms") == 0){
    desc.title = "Environment: TwoRooms\n";
    e = new TwoRooms(rng, stochastic, true, delay, false);
  } else if (strcmp(envType, "hectorquad") == 0){
    desc.title = "Environment: HectorQuad\n";
    e = new HectorQuad(rng);
  } else {
    std::cerr << "Invalid env type" << std::endl;
    displayHelp();
    exit(-1);
  }

  // fill in some more description info
  desc.num_actions = e->getNumActions();
  desc.episodic = e->isEpisodic();

  std::vector<float> maxFeats;
  std::vector<float> minFeats;

  e->getMinMaxFeatures(&minFeats, &maxFeats);
  desc.num_states = minFeats.size();
  desc.min_state_range = minFeats;
  desc.max_state_range = maxFeats;

  desc.stochastic = stochastic;
  float minReward;
  float maxReward;
  e->getMinMaxReward(&minReward, &maxReward);
  desc.max_reward = maxReward;
  desc.reward_range = maxReward - minReward;

  std::cout << desc.title << std::endl;

  // publish environment description
  out_env_desc.publish(desc);

  sleep(1);

  // send experiences
  std::vector<experience> seeds = e->getSeedings();
  for (unsigned i = 0; i < seeds.size(); i++){
    rl_msgs::RLEnvSeedExperience seed;
    seed.from_state = seeds[i].s;
    seed.to_state   = seeds[i].next;
    seed.action     = seeds[i].act;
    seed.reward     = seeds[i].reward;
    seed.terminal   = seeds[i].terminal;
    out_seed.publish(seed);
  }

  // now send first state message
  rl_msgs::RLStateReward sr;
  sr.terminal = false;
  sr.reward = 0;
  sr.state = e->sensation();
  out_env_sr.publish(sr);

}


/** Main function to start the env node. */
int main(int argc, char *argv[])
{
  ros::init(argc, argv, NODE);
  ros::NodeHandle node;

  if (argc < 2){
    std::cout << "--env type  option is required" << std::endl;
    displayHelp();
    exit(-1);
  }

  // env and seed are required
  if (argc < 3){
    displayHelp();
    exit(-1);
  }

  // parse options to change these parameters
  envType = argv[1];
  seed = std::atoi(argv[2]);

  // parse env type first
  bool gotEnv = false;
  for (int i = 1; i < argc-1; i++){
    if (strcmp(argv[i], "--env") == 0){
      gotEnv = true;
      envType = argv[i+1];
    }
  }
  if (!gotEnv) {
    std::cout << "--env type  option is required" << std::endl;
    displayHelp();
  }

  // now parse other options
  char ch;
  const char* optflags = "edsaxp";
  int option_index = 0;
  static struct option long_options[] = {
    {"env", 1, 0, 'e'},
    {"deterministic", 0, 0, 'd'},
    {"stochastic", 0, 0, 's'},
    {"delay", 1, 0, 'a'},
    {"seed", 1, 0, 'x'},
    {"prints", 1, 0, 'p'},
    {NULL, 0, 0, 0}
  };

  while(-1 != (ch = getopt_long_only(argc, argv, optflags, long_options, &option_index))) {
    switch(ch) {

    case 'x':
      seed = std::atoi(optarg);
      std::cout << "seed: " << seed << std::endl;
      break;

    case 'd':
      stochastic = false;
      std::cout << "stochastic: " << stochastic << std::endl;
      break;

    case 's':
      stochastic = true;
      std::cout << "stochastic: " << stochastic << std::endl;
      break;

    case 'a':
      {
        if (strcmp(envType, "mcar") == 0 || strcmp(envType, "tworooms") == 0){
          delay = std::atoi(optarg);
          std::cout << "delay steps: " << delay << std::endl;
        } else {
          std::cout << "--delay option is only valid for the mcar and "
                       "tworooms domains" << std::endl;
          exit(-1);
        }
        break;
      }

    case 'e':
      // already processed this one
      std::cout << "env: " << envType << std::endl;
      break;

    case 'p':
      PRINTS = std::atoi(optarg);
      break;

    case 'h':
    case '?':
    case 0:
    default:
      displayHelp();
      break;
    }
  }


  int qDepth = 1;

  // Set up Publishers
  std::cout << NODE << " Initializing ROS ..." << std::endl;
  ros::init(argc, argv, "my_tf_broadcaster");
  tf::Transform transform;

  std::cout << NODE << " Setting up publishers ..." << std::endl;
  out_env_desc = node.advertise<rl_msgs::RLEnvDescription>(
    "rl_env/rl_env_description",
    qDepth,
    true);
  out_env_sr = node.advertise<rl_msgs::RLStateReward>(
    "rl_env/rl_state_reward",
    qDepth,
    false);
  out_seed = node.advertise<rl_msgs::RLEnvSeedExperience>(
    "rl_env/rl_seed",
    20,
    false);

  std::cout << NODE << " Setting up subscribers ...\n";
  ros::TransportHints noDelay = ros::TransportHints().tcpNoDelay(true);
  ros::Subscriber rl_action =  node.subscribe("rl_agent/rl_action",
                                              qDepth,
                                              processAction,
                                              noDelay);
  ros::Subscriber rl_exp_info =  node.subscribe("rl_agent/rl_experiment_info",
                                                qDepth,
                                                processEpisodeInfo,
                                                noDelay);

  // publish env description, first state
  std::cout << NODE << " Initializing the environment ..." << std::endl;
  rng = Random(1+seed);
  initEnvironment();

  ROS_INFO(NODE ": starting main loop");

  ros::spin();

  return 0;
}
