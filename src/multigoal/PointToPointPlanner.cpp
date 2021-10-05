//
// Created by werner on 10/1/21.
//

#include "PointToPointPlanner.h"
#include "../UnionGoalSampleableRegion.h"
#include "multi_goal_planners.h"


PointToPointPlanner::PointToPointPlanner(const ompl::base::PlannerPtr &planner,
                                         const std::shared_ptr<ompl::base::OptimizationObjective> &optimizationObjective,
                                         const robowflex::RobotPtr robot)
        : planner_(planner), optimizationObjective_(optimizationObjective), robot_(robot) {}

std::optional<PointToPointPlanResult>
PointToPointPlanner::planPointToPoint(const moveit::core::RobotState &from_state, const Eigen::Vector3d &target,
                                      double maxTime) {

    std::vector<Eigen::Vector3d> targets{target};
    return this->planPointToPoint(from_state, targets, maxTime);
}

std::optional<PointToPointPlanResult> PointToPointPlanner::planPointToPoint(const moveit::core::RobotState &from_state,
                                                                            const std::vector<Eigen::Vector3d> &targets,
                                                                            double maxTime) {

    // Construct the OMPL goal from the set of targets.
    ompl::base::GoalPtr goal = this->constructUnionGoal(targets);

    // Predeclare the result as an std::optional
    std::optional<PointToPointPlanResult> result;

    // Set the problem definition, including start state, goal and optimization objective.
    auto ptr = constructProblemDefinition(from_state, goal);
    planner_->setProblemDefinition(ptr);

    // We explicitly do the setup beforehand to avoid counting it in the benchmarking.
    if (!planner_->isSetup()) planner_->setup();

    // Run the OMPL planner, record the time taken.
    std::chrono::steady_clock::time_point pre_solve = std::chrono::steady_clock::now();
    ompl::base::PlannerStatus status = planner_->solve(ompl::base::timedPlannerTerminationCondition(maxTime));
    std::chrono::steady_clock::time_point post_solve = std::chrono::steady_clock::now();

    // TODO: Store this in the statistics.
    long elapsed_millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            (post_solve - pre_solve)).count();

    // "Approximate" solutions can be wildly off, so we accept exact solutions only.
    if (status == ompl::base::PlannerStatus::EXACT_SOLUTION) {

        // Initialize an empty trajectory.
        auto trajectory = this->convertTrajectory(*ptr->getSolutionPath()->as<ompl::geometric::PathGeometric>());

        const Eigen::Vector3d end_eepos = trajectory.getTrajectory()->getLastWayPoint().getGlobalLinkTransform(
                "end_effector").translation();

        for (size_t i = 0; i < targets.size(); i++) {
            auto tgt = targets[i];
            if ((tgt - end_eepos).norm() < GOAL_END_EFFECTOR_RADIUS) { // FIXME Don't use magic numbers!
                result = {PointToPointPlanResult{
                        .solution_length = trajectory.getLength(),
                        .point_to_point_trajectory = trajectory,
                        .endEffectorTarget = tgt,
                        .ith_target = i,
                }};
                break;
            }
        }

        assert(result.has_value());

    } else {
        result = {};
        std::cout << "Apple unreachable" << std::endl;
    }

    planner_->clearQuery();

    return result;

}

std::shared_ptr<ompl::base::ProblemDefinition>
PointToPointPlanner::constructProblemDefinition(const moveit::core::RobotState &from_state,
                                                const ompl::base::GoalPtr &goal) const {
    ompl::base::ScopedState start(planner_->getSpaceInformation());
    planner_->getSpaceInformation()->getStateSpace()->as<DroneStateSpace>()->copyToOMPLState(start.get(), from_state);
    auto pdef = std::make_shared<ompl::base::ProblemDefinition>(planner_->getSpaceInformation());
    pdef->addStartState(start.get());
    pdef->setOptimizationObjective(optimizationObjective_);
    pdef->setGoal(goal);
    return pdef;
}

ompl::base::GoalPtr PointToPointPlanner::constructUnionGoal(const std::vector<Eigen::Vector3d> &targets) {
    if (targets.size() == 1) {
        return std::make_shared<DroneEndEffectorNearTarget>(planner_->getSpaceInformation(), GOAL_END_EFFECTOR_RADIUS,
                                                            targets[0]);
    } else {
        std::vector<std::shared_ptr<const ompl::base::GoalSampleableRegion>> subgoals;

        for (const auto &target: targets) {
            subgoals.push_back(
                    std::make_shared<DroneEndEffectorNearTarget>(planner_->getSpaceInformation(),
                                                                 GOAL_END_EFFECTOR_RADIUS, target));
        }

        return std::make_shared<UnionGoalSampleableRegion>(planner_->getSpaceInformation(), subgoals);
    }
}

robowflex::Trajectory PointToPointPlanner::convertTrajectory(ompl::geometric::PathGeometric &path) {
    // Initialize an empty trajectory.
    robowflex::Trajectory trajectory(robot_, "whole_body");

    moveit::core::RobotState st(robot_->getModelConst());

    auto state_space = planner_->getSpaceInformation()->getStateSpace()->as<DroneStateSpace>();

    for (auto state: path.getStates()) {
        state_space->copyToRobotState(st, state);
        trajectory.addSuffixWaypoint(st);
    }

    return trajectory;
}

const ompl::base::PlannerPtr &PointToPointPlanner::getPlanner() const {
    return planner_;
}

const std::shared_ptr<ompl::base::OptimizationObjective> &PointToPointPlanner::getOptimizationObjective() const {
    return optimizationObjective_;
}
