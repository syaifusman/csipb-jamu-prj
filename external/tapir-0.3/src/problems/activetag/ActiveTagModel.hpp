/** @file ActiveTagModel.hpp
 *
 * Contains ActiveTagModel, which implements the core Model interface for the ActiveTag POMDP.
 */
#ifndef ACTIVETAGMODEL_HPP_
#define ACTIVETAGMODEL_HPP_

#include <memory>                       // for unique_ptr
#include <ostream>                      // for ostream
#include <string>                       // for string
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "global.hpp"                     // for RandomGenerator

#include "problems/shared/GridPosition.hpp"  // for GridPosition
#include "problems/shared/ModelWithProgramOptions.hpp"  // for ModelWithProgramOptions

#include "solver/abstract-problem/Model.hpp"             // for Model::StepResult, Model
#include "solver/abstract-problem/ModelChange.hpp"             // for ModelChange
#include "solver/abstract-problem/TransitionParameters.hpp"
#include "solver/abstract-problem/Action.hpp"            // for Action
#include "solver/abstract-problem/Observation.hpp"       // for Observation
#include "solver/abstract-problem/State.hpp"

#include "solver/mappings/actions/enumerated_actions.hpp"
#include "solver/mappings/observations/discrete_observations.hpp"

#include "ActiveTagAction.hpp"
#include "ActiveTagOptions.hpp"
#include "ActiveTagMdpSolver.hpp"

namespace solver {
class StatePool;
} /* namespace solver */

/** A namespace to hold the various classes used for the ActiveTag POMDP model. */
namespace activetag {
class ActiveTagObservation;
class ActiveTagState;

/** Represents a change in the ActiveTag model. */
struct ActiveTagChange : public solver::ModelChange {
    /** The change type for this change - this should be one of:
     * - "Add Obstacles" - to add new obstacles to the ActiveTag problem.
     * - "Remove Obstacles" - to remove existing obstacles from the ActiveTag problem.
     */
    std::string changeType = "";
    /** The first row number where the change applies. */
    long i0 = 0;
    /** The last row number where the change applies. */
    long i1 = 0;
    /** The first column number where the change applies. */
    long j0 = 0;
    /** The last column number where the change applies. */
    long j1 = 0;
};

/** A parser for a simple upper bound heuristic for ActiveTag.
 *
 * The actual function is defined in ActiveTagModel::getUpperBoundHeuristicValue; this parser allows
 * that heuristic to be selected by using the string "upper()" in the configuration file.
 */
class ActiveTagUBParser : public shared::Parser<solver::HeuristicFunction> {
public:
    /** Creates a new ActiveTagUBParser associated with the given ActiveTagModel instance. */
    ActiveTagUBParser(ActiveTagModel *model);
    virtual ~ActiveTagUBParser() = default;
    _NO_COPY_OR_MOVE(ActiveTagUBParser);

    virtual solver::HeuristicFunction parse(solver::Solver *solver, std::vector<std::string> args);

private:
    /** The ActiveTagModel instance this heuristic parser is associated with. */
    ActiveTagModel *model_;
};

/** The implementation of the Model interface for the ActiveTag POMDP.
 *
 * See this paper http://www.cs.cmu.edu/~ggordon/jpineau-ggordon-thrun.ijcai03.pdf
 * for a description of the Tag problem.
 *
 * This class inherits from shared::ModelWithProgramOptions in order to use custom text-parsing
 * functionality to select many of the core ABT parameters, allowing the configuration options
 * to be changed easily via the configuration interface without having to recompile the code.
 */
class ActiveTagModel: public shared::ModelWithProgramOptions {
    friend class TagObservation;
    friend class ActiveTagMdpSolver;

  public:
    /** Constructs a new ActiveTagModel instance with the given random number engine, and the given set
     * of configuration options.
     */
    ActiveTagModel(RandomGenerator *randGen, std::unique_ptr<ActiveTagOptions> options);

    ~ActiveTagModel() = default;
    _NO_COPY_OR_MOVE(ActiveTagModel);

    /** The cells are either empty or walls. */
    enum class TagCellType : int {
        /** An empty cell. */
        EMPTY = 0,
        /* A wall. */
        WALL = -1
    };

    /** Get a vector of valid grid positions */
    std::vector<GridPosition> getEmptyCells();

    /******************** Added by Josh **************************/

    /** Get 2D vector representing the current environment map */
    inline const std::vector<std::vector<TagCellType>>& getEnvMap() {
        return envMap_;
    }

    /**
     * Returns proportion of belief particles about the target's
     * position for each grid position in the map
     */
    std::vector<std::vector<float>> getBeliefProportions(solver::BeliefNode const *belief);

    /************************************************************/

    /** Returns the resulting coordinates of an agent after it takes the given action type from the
     * given position.
     *
     * The boolean flag will be false if the agent's move represents an attempt to move into an
     * obstacle or off the edge of the map, which in the ActiveTag POMDP simply causes them to stay
     * in the same position.
     *
     * This flag is mostly not used as there is no penalty for this in ActiveTag, and the returned
     * position already reflects them staying still.
     */
    std::pair<GridPosition, bool> getMovedPos(GridPosition const &position, ActionType action);

    /** Generates a new opponent position based on the current positions of the robot and the
     * opponent.
     */
    GridPosition sampleNextOpponentPosition(GridPosition const &robotPos,
            GridPosition const &opponentPos);

    /* ---------- Custom getters for extra functionality  ---------- */
    /** Returns the number of rows in the map for this ActiveTagModel instance. */
    long getNRows() const {
        return nRows_;
    }
    /** Returns the number of columns in the map for this ActiveTagModel instance. */
    long getNCols() const {
        return nCols_;
    }

    /** Initializes a ActiveTagMdpSolver for this ActiveTagModel, which can then be used to return heuristic
     * values for each state.
     */
    void makeMdpSolver() {
        mdpSolver_ = std::make_unique<ActiveTagMdpSolver>(this);
        mdpSolver_->solve();
    }

    /** Returns the ActiveTagMdpSolver solver (if any) owned by this model. */
    ActiveTagMdpSolver *getMdpSolver() {
        return mdpSolver_.get();
    }

    /** Returns the distance within the map between the two given positions. */
    int getMapDistance(GridPosition p1, GridPosition p2);


    /* --------------- The model interface proper ----------------- */
    std::unique_ptr<solver::State> sampleAnInitState() override;
    std::unique_ptr<solver::State> sampleStateUninformed() override;
    bool isTerminal(solver::State const &state) override;
    bool isValid(solver::State const &state) override;

    /* -------------------- Black box dynamics ---------------------- */
    virtual std::unique_ptr<solver::State> generateNextState(
            solver::State const &state,
            solver::Action const &action,
            solver::TransitionParameters const * /*tp*/) override;
    virtual std::unique_ptr<solver::Observation> generateObservation(
            solver::State const * /*state*/,
            solver::Action const &action,
            solver::TransitionParameters const * /*tp*/,
            solver::State const &nextState) override;
    virtual double generateReward(
                solver::State const &state,
                solver::Action const &action,
                solver::TransitionParameters const * /*tp*/,
                solver::State const * /*nextState*/) override;
    virtual Model::StepResult generateStep(solver::State const &state,
            solver::Action const &action) override;


    /* -------------- Methods for handling model changes ---------------- */
    virtual void applyChanges(std::vector<std::unique_ptr<solver::ModelChange>> const &changes,
             solver::Solver *solver) override;


    /* ------------ Methods for handling particle depletion -------------- */
    /** Generates particles for ActiveTag using a particle filter from the previous belief.
      *
      * For each previous particle, possible next states are calculated based on consistency with
      * the given action and observation. These next states are then added to the output vector
      * in accordance with their probability of having been generated.
      */
    virtual std::vector<std::unique_ptr<solver::State>> generateParticles(
            solver::BeliefNode *previousBelief,
            solver::Action const &action,
            solver::Observation const &obs,
            long nParticles,
            std::vector<solver::State const *> const &previousParticles) override;

    /** Generates particles for ActiveTag according to an uninformed prior.
     *
     * Previous states are sampled uniformly at random, a single step is generated, and only states
     * consistent with the action and observation are kept.
     */
    virtual std::vector<std::unique_ptr<solver::State>> generateParticles(
            solver::BeliefNode *previousBelief,
            solver::Action const &action,
            solver::Observation const &obs,
            long nParticles) override;


    /* --------------- Pretty printing methods ----------------- */
    /** Prints a single cell of the map out to the given output stream. */
    virtual void dispCell(TagCellType cellType, std::ostream &os);
    virtual void drawEnv(std::ostream &os) override;
    virtual void drawSimulationState(solver::BeliefNode const *belief,
            solver::State const &state,
            std::ostream &os) override;


    /* ---------------------- Basic customizations  ---------------------- */
    virtual double getDefaultHeuristicValue(solver::HistoryEntry const *entry,
                solver::State const *state, solver::HistoricalData const *data) override;

    /** Returns an upper bound heuristic value for the given state.
     *
     * This upper bound assumes that the opponent will not move, and hence the heuristic value
     * simply calculates the discounted total reward, including the cost of moving one square at a
     * time until the robot reaches the opponent's current square, and then the reward for tagging
     * the opponent at that time.
     */
    virtual double getUpperBoundHeuristicValue(solver::State const &state);

    /* ------- Customization of more complex solver functionality  --------- */
    /** Returns all of the actions available for the ActiveTag POMDP, in the order of their enumeration
     * (as specified by activetag::ActionType).
     */
    virtual std::vector<std::unique_ptr<solver::DiscretizedPoint>> getAllActionsInOrder();
    virtual std::unique_ptr<solver::ActionPool> createActionPool(solver::Solver *solver) override;

    virtual std::unique_ptr<solver::Serializer> createSerializer(solver::Solver *solver) override;

  private:
    /** Calculates the distances from the given position to all other parts of the map. */
    void calculateDistancesFrom(GridPosition position);
    /** Calculates all pairwise distances on the map. */
    void calculatePairwiseDistances();

    /** Initialises the required data structures and variables for this model. */
    void initialize();

    /** Generates a random empty grid cell. */
    GridPosition randomEmptyCell();

    /** Generates a next state for the given state and action, as well as a boolean flag that will
     * be true if the action moved into a wall, and false otherwise.
     *
     * Moving into a wall in ActiveTag simply means nothing happens - there is no associated penalty;
     * as such, this flag is mostly not used in the ActiveTag problem.
     */
    std::pair<std::unique_ptr<ActiveTagState>, bool> makeNextState(
            solver::State const &state, solver::Action const &action);

    /** Generates an observation given the resulting next state, after the ActiveTag robot has made its
     * action.
     */
    std::unique_ptr<solver::Observation> makeObservation(ActiveTagState const &nextState);

    /** Generates a distribution of possible actions the opponent may choose to take, based on the
     * current position of the robot and the opponent.
     *
     * This distribution is represented by a vector of four elements, because the opponent's
     * actions are always evenly distributed between four possibilities (some of which can be the
     * same).
     */
    std::vector<ActionType> makeOpponentActions(GridPosition const &robotPos,
            GridPosition const &opponentPos);

    /** Generates a proper distribution for the possible positions the opponent could be in
     * after the current state.
     */
    std::unordered_map<GridPosition, double> getNextOpponentPositionDistribution(
            GridPosition const &robotPos, GridPosition const &opponentPos);

    /** Returns true iff the given GridPosition represents a valid square that an agent could be
     * in - that is, the square must be empty, and within the bounds of the map.
     */
    bool isValid(GridPosition const &pos);

    /** The ActiveTagOptions instance associated with this model. */
    ActiveTagOptions *options_;

    /** The penalty for each movement action. */
    double moveCost_;
    /** The reward for successfully tagging the opponent. */
    double tagReward_;
    /** The penalty for failing a activetag attempt. */
    double failedTagPenalty_;
    /** The probability that the opponent will stay still. */
    double opponentStayProbability_;

    /** The number of rows in the map. */
    long nRows_;
    /** The number of columns in the map. */
    long nCols_;

    /** The environment map in text form. */
    std::vector<std::string> mapText_;
    /** The environment map in vector form. */
    std::vector<std::vector<TagCellType>> envMap_;

    /** The number of possible actions in the ActiveTag POMDP. */
    long nActions_;

    /** Solver for the MDP version of the problem. */
    std::unique_ptr<ActiveTagMdpSolver> mdpSolver_;

    /** The pairwise distances between each pair of cells in the map. */
    std::vector<std::vector<std::vector<std::vector<int>>>> pairwiseDistances_;
};
} /* namespace activetag */

#endif /* ACTIVETAGMODEL_HPP_ */
