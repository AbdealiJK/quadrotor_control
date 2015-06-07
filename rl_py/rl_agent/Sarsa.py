import random
import pickle

from rl_agent.Agent import Agent


class Sarsa(Agent):
    """
    Agent that uses straight Sarsa Lambda, with no generalization and
    epsilon-greedy exploration.
    """
    def __init__(self, num_actions, gamma, initialvalue, alpha, epsilon, _lambda,
                 rng=random.Random()):
        """
        Standard constructor

        :param numactions:   The number of possible actions.
        :param gamma:        The discount factor.
        :param initialvalue: The initial value of each Q(s,a).
        :param alpha:        The learning rate.
        :param epsilon:      The probability of taking a random action.
        :param rng:          Random number generator to use.
        """
        self.num_actions = int(num_actions)
        self.gamma = gamma
        self.initialvalue = initialvalue
        self.alpha = alpha
        self.epsilon = epsilon
        self._lambda = _lambda
        self.rng = rng

        self.ACTDEBUG = False
        self.ELIGDEBUG = False

        self.eligibility = {}  # Key = sensation, values = trace of each action
        self.Q = {}  # Key = sensation, values = value of each action

    def first_action(self, sensation):
        if self.ACTDEBUG:
            print("First - in state: ", ",".join(sensation))

        self.sparse_populate(sensation)

        # Clear eligibility traces
        for state in self.eligibility:
            self.eligibility[state] = [0.0] * self.num_actions

        # Get action values
        if self.rng.random() < self.epsilon: # Choose randomly
            a = self.rng.randint(0, self.num_actions - 1)
        else: # Choose maximum
            a = self.random_max_element(self.Q[sensation])

        self.eligibility[sensation][a] = 1.0

        if self.ACTDEBUG:
            print("MAX - act:", a, "val:", self.Q[sensation][a])
            for i_act, act_val in enumerate(self.Q[sensation]):
                print("Action:", i_act, "val:", act_val)
            print("Took action", a, "from state", ",".join(sensation))

        return a;

    def next_action(self, reward, sensation):
        if self.ACTDEBUG:
            print("Next: got reward", reward, "in state:", ",".join(sensation))

        self.sparse_populate(sensation)

        # Get action values
        if self.rng.random() < self.epsilon: # Choose randomly
            a = self.rng.randint(0, self.num_actions - 1)
        else: # Choose maximum
            a = self.random_max_element(self.Q[sensation])

        # Update value for all with positive eligibility
        for state in self.eligibility:
            for i_act in xrange(self.num_actions):
                if self.eligibility[state][i_act] > 0.0:
                    if self.ELIGDEBUG:
                        print("updating state:", ",".join(state),
                              "act:", i_act,
                              "with elig:", self.eligibility[state][i_act])
                    self.Q[state][i_act] += self.alpha * \
                        self.eligibility[state][i_act] * \
                        (reward + self.gamma * self.Q[sensation][a] - self.Q[state][i_act])
                    self.eligibility[state][i_act] *= self._lambda

        self.eligibility[sensation][a] = 1.0

        if self.ACTDEBUG:
            print("MAX - act:", a, "val:", self.Q[sensation][a])
            for i_act, act_val in enumerate(self.Q[sensation]):
                print("Action:", i_act, "val:", act_val)
            print("Took action", a, "from state", ",".join(sensation))

        return a

    def last_action(self, reward):
        if self.ACTDEBUG:
            print("Last: got reward", reward)

        # Update value for all with positive eligibility
        for state in self.eligibility:
            for i_act in xrange(self.num_actions):
                if self.eligibility[state][i_act] > 0.0:
                    if self.ELIGDEBUG:
                        print("updating state:", ",".join(state),
                              "act:", i_act,
                              "with elig:", self.eligibility[state][i_act])
                    self.Q[state][i_act] += self.alpha * \
                        self.eligibility[state][i_act] * \
                        (reward - self.Q[state][i_act])
                    self.eligibility[state][i_act] = 0.0

    def set_debug(self, debug):
        self.ACTDEBUG = debug

    def seed_exp(self, seeds):
        # For each seeding experience, update our model
        for i_exp, exp in enumerate(seeds):
            val_act = self.Q[exp.s][exp.act]

            # Update value of action just executed
            self.Q[exp.s][exp.act] += alpha * \
                (exp.reward + self.gamma * val_act - self.Q[exp.s][e.act])

            print("Seeding with experience" << i_exp)
            print("last:", ",".join(exp.s))
            print("act:", exp.act, "reward:", exp.reward)
            print("act:", exp.act, "reward:", exp.reward)
            print("next:", ",".join(exp.next), "terminal:", exp.terminal)

    def save_policy(self, filename):
        with open(filename, "wb") as f:
            pickle.dump(self, f)

    def get_value(self, state):
        """
        Gets the value fo a given state

        :param state: State to find out about
        :return:      Value of the state - float
        """
        a = self.random_max_element(self.Q[state])

        val_sum = 0.0
        val_count = 0
        for state in self.Q:
            val_sum += sum(self.Q[state])
            val_count += len(self.Q[state])
        print("Avg Value:", val_sum / val_count)

        return self.Q[state][a]

    def random_max_element(self, seq):
        """
        Finds a random max element from a given list

        :param seq: List of elements which are comparable
        :return:    A random max element
        """
        if not seq: return []
        max_val = seq[0] if seq[0] >= seq[-1] else seq[-1]
        max_indices = []
        for i, val in enumerate(seq):
            if val < max_val: continue
            if val == max_val:
                max_indices.append(i)
            else:
                max_val = val
                max_indices = [i]
        return self.rng.choice(max_indices)

    def log_values(filename, xmin, xmax, ymin, ymax):
        raise NotImplementedError()

    def sparse_populate(self, state):
        """
        Call this before trying to find a new state in the Q function
        """
        try:
            self.Q[state]
        except KeyError:
            self.Q[state] = [self.initialvalue] * self.num_actions
        try:
            self.eligibility[state]
        except KeyError:
            self.eligibility[state] = [0.0] * self.num_actions

