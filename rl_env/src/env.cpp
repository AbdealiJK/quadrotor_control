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

#include <rl_env/RobotCarVel.hh>
#include <rl_env/fourrooms.hh>
#include <rl_env/tworooms.hh>
#include <rl_env/taxi.hh>
#include <rl_env/FuelRooms.hh>
#include <rl_env/stocks.hh>
#include <rl_env/energyrooms.hh>
#include <rl_env/MountainCar.hh>
#include <rl_env/CartPole.hh>
#include <rl_env/LightWorld.hh>
#include <rl_env/HectorQuad.hh>

#include <getopt.h>
#include <stdlib.h>

#define NODE "RLEnvironment"

static ros::Publisher out_env_desc;
static ros::Publisher out_env_sr;
static ros::Publisher out_seed;

Environment* e;
Random rng;
bool PRINTS = false;//true;
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
  std::cout << "\n Call env --env type [options]\n";
  std::cout << "Env types: taxi tworooms fourrooms energy fuelworld mcar cartpole car2to7 car7to2 carrandom stocks lightworld hectorquad\n";
  std::cout << "\n Options:\n";
  std::cout << "--seed value (integer seed for random number generator)\n";
  std::cout << "--deterministic (deterministic version of domain)\n";
  std::cout << "--stochastic (stochastic version of domain)\n";
  std::cout << "--delay value (# steps of action delay (for mcar and tworooms)\n";
  std::cout << "--lag (turn on brake lag for car driving domain)\n";
  std::cout << "--highvar (have variation fuel costs in Fuel World)\n";
  std::cout << "--nsectors value (# sectors for stocks domain)\n";
  std::cout << "--nstocks value (# stocks for stocks domain)\n";
  std::cout << "--prints (turn on debug printing of actions/rewards)\n";

  std::cout << "\n For more info, see: http://www.ros.org/wiki/rl_env\n";
  exit(-1);
}


/** process action from the agent */
void processAction(const rl_msgs::RLAction::ConstPtr &actionIn){

  rl_msgs::RLStateReward sr;

  // process action from the agent, affecting the environment
  sr.reward = e->apply(actionIn->action);
  sr.state = e->sensation();
  sr.terminal = e->terminal();

  // publish the state-reward message
  if (PRINTS) std::cout << "Got action " << actionIn->action << " at state: " << sr.state[0] << ", " << sr.state[1] << ", reward: " << sr.reward << endl;

  out_env_sr.publish(sr);

}

/** Process end-of-episode reward info.
    Mostly to start new episode. */
void processEpisodeInfo(const rl_msgs::RLExperimentInfo::ConstPtr &infoIn){
  // start new episode if terminal
  if (PRINTS) std::cout << "Episode " << infoIn->episode_number << " terminated with reward: " << infoIn->episode_reward << ", start new episode " << endl;

  e->reset();

  rl_msgs::RLStateReward sr;
  sr.reward = 0;
  sr.state = e->sensation();
  sr.terminal = false;
  out_env_sr.publish(sr);
}


/** init the environment, publish a description. */
void initEnvironment(){

  // init the environment
  e = NULL;
  rl_msgs::RLEnvDescription desc;

  if (strcmp(envType, "cartpole") == 0){
    desc.title = "Environment: Cart Pole\n";
    e = new CartPole(rng, stochastic);
  }

  else if (strcmp(envType, "mcar") == 0){
    desc.title = "Environment: Mountain Car\n";
    e = new MountainCar(rng, stochastic, false, delay);
  }

  // taxi
  else if (strcmp(envType, "taxi") == 0){
    desc.title = "Environment: Taxi\n";
    e = new Taxi(rng, stochastic);
  }

  // Light World
  else if (strcmp(envType, "lightworld") == 0){
    desc.title = "Environment: Light World\n";
    e = new LightWorld(rng, stochastic, 4);
  }

  // two rooms
  else if (strcmp(envType, "tworooms") == 0){
    desc.title = "Environment: TwoRooms\n";
    e = new TwoRooms(rng, stochastic, true, delay, false);
  }

  // car vel, 2 to 7
  else if (strcmp(envType, "car2to7") == 0){
    desc.title = "Environment: Car Velocity 2 to 7 m/s\n";
    e = new RobotCarVel(rng, false, true, false, lag);
  }
  // car vel, 7 to 2
  else if (strcmp(envType, "car7to2") == 0){
    desc.title = "Environment: Car Velocity 7 to 2 m/s\n";
    e = new RobotCarVel(rng, false, false, false, lag);
  }
  // car vel, random vels
  else if (strcmp(envType, "carrandom") == 0){
    desc.title = "Environment: Car Velocity Random Velocities\n";
    e = new RobotCarVel(rng, true, false, false, lag);
  }

  // four rooms
  else if (strcmp(envType, "fourrooms") == 0){
    desc.title = "Environment: FourRooms\n";
    e = new FourRooms(rng, stochastic, true, false);
  }

  // four rooms with energy level
  else if (strcmp(envType, "energy") == 0){
    desc.title = "Environment: EnergyRooms\n";
    e = new EnergyRooms(rng, stochastic, true, false);
  }

  // gridworld with fuel (fuel stations on top and bottom with random costs)
  else if (strcmp(envType, "fuelworld") == 0){
    desc.title = "Environment: FuelWorld\n";
    e = new FuelRooms(rng, highvar, stochastic);
  }

  // stocks
  else if (strcmp(envType, "stocks") == 0){
    desc.title = "Environment: Stocks\n";
    e = new Stocks(rng, stochastic, nsectors, nstocks);
  }

  // hector_quadrotor
  else if (strcmp(envType, "hectorquad") == 0){
    desc.title = "Environment: Quadrotor\n";
    // Set up a subscriber
    e = new HectorQuad(rng);
  }

  else {
    std::cerr << "Invalid env type" << endl;
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

  std::cout << desc.title << endl;

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
    std::cout << "--env type  option is required" << endl;
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
    std::cout << "--env type  option is required" << endl;
    displayHelp();
  }

  // now parse other options
  char ch;
  const char* optflags = "ds:";
  int option_index = 0;
  static struct option long_options[] = {
    {"env", 1, 0, 'e'},
    {"deterministic", 0, 0, 'd'},
    {"stochastic", 0, 0, 's'},
    {"delay", 1, 0, 'a'},
    {"nsectors", 1, 0, 'c'},
    {"nstocks", 1, 0, 't'},
    {"lag", 0, 0, 'l'},
    {"nolag", 0, 0, 'o'},
    {"seed", 1, 0, 'x'},
    {"prints", 0, 0, 'p'},
    {"highvar", 0, 0, 'v'}
  };

  while(-1 != (ch = getopt_long_only(argc, argv, optflags, long_options, &option_index))) {
    switch(ch) {

    case 'x':
      seed = std::atoi(optarg);
      std::cout << "seed: " << seed << endl;
      break;

    case 'd':
      stochastic = false;
      std::cout << "stochastic: " << stochastic << endl;
      break;

    case 'v':
      {
        if (strcmp(envType, "fuelworld") == 0){
          highvar = true;
          std::cout << "fuel world fuel cost variation: " << highvar << endl;
        } else {
          std::cout << "--highvar is only a valid option for the fuelworld domain." << endl;
          exit(-1);
        }
        break;
      }

    case 's':
      stochastic = true;
      std::cout << "stochastic: " << stochastic << endl;
      break;

    case 'a':
      {
        if (strcmp(envType, "mcar") == 0 || strcmp(envType, "tworooms") == 0){
          delay = std::atoi(optarg);
          std::cout << "delay steps: " << delay << endl;
        } else {
          std::cout << "--delay option is only valid for the mcar and tworooms domains" << endl;
          exit(-1);
        }
        break;
      }

    case 'c':
      {
        if (strcmp(envType, "stocks") == 0){
          nsectors = std::atoi(optarg);
          std::cout << "nsectors: " << nsectors << endl;
        } else {
          std::cout << "--nsectors option is only valid for the stocks domain" << endl;
          exit(-1);
        }
        break;
      }
    case 't':
      {
        if (strcmp(envType, "stocks") == 0){
          nstocks = std::atoi(optarg);
          std::cout << "nstocks: " << nstocks << endl;
        } else {
          std::cout << "--nstocks option is only valid for the stocks domain" << endl;
          exit(-1);
        }
        break;
      }

    case 'l':
      {
        if (strcmp(envType, "car2to7") == 0 || strcmp(envType, "car7to2") == 0 || strcmp(envType, "carrandom") == 0){
          lag = true;
          std::cout << "lag: " << lag << endl;
        } else {
          std::cout << "--lag option is only valid for car velocity tasks" << endl;
          exit(-1);
        }
        break;
      }

    case 'o':
       {
         if (strcmp(envType, "car2to7") == 0 || strcmp(envType, "car7to2") == 0 || strcmp(envType, "carrandom") == 0){
           lag = false;
           std::cout << "lag: " << lag << endl;
         } else {
           std::cout << "--nolag option is only valid for car velocity tasks" << endl;
           exit(-1);
         }
         break;
       }

    case 'e':
      // already processed this one
      std::cout << "env: " << envType << endl;
      break;

    case 'p':
      PRINTS = true;
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
  std::cout << NODE << " Initializing ROS ...\n";
  ros::init(argc, argv, "my_tf_broadcaster");
  tf::Transform transform;

  std::cout << NODE << " Setting up publishers ...\n";
  out_env_desc = node.advertise<rl_msgs::RLEnvDescription>("rl_env/rl_env_description",qDepth,true);
  out_env_sr = node.advertise<rl_msgs::RLStateReward>("rl_env/rl_state_reward",qDepth,false);
  out_seed = node.advertise<rl_msgs::RLEnvSeedExperience>("rl_env/rl_seed",20,false);

  std::cout << NODE << " Setting up subscribers ...\n";
  ros::TransportHints noDelay = ros::TransportHints().tcpNoDelay(true);
  ros::Subscriber rl_action =  node.subscribe("rl_agent/rl_action", qDepth, processAction, noDelay);
  ros::Subscriber rl_exp_info =  node.subscribe("rl_agent/rl_experiment_info", qDepth, processEpisodeInfo, noDelay);

  // publish env description, first state
  std::cout << NODE << " Initializing the environment ...\n";
  rng = Random(1+seed);
  initEnvironment();

  ROS_INFO(NODE ": starting main loop");

  ros::spin();                          // handle incoming data

  return 0;
}




