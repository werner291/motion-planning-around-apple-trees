
#include "DistanceHeuristics.h"

#include <utility>

double EuclideanOmplDistanceHeuristics::state_to_goal(const ompl::base::State *a, const ompl::base::Goal *b) const {
    moveit::core::RobotState sta(state_space_->getRobotModel());

    state_space_->copyToRobotState(sta, a);

    Eigen::Vector3d end_effector_pos = sta.getGlobalLinkTransform("end_effector").translation();

    return (end_effector_pos - b->as<DroneEndEffectorNearTarget>()->getTarget()).norm();
}

double EuclideanOmplDistanceHeuristics::goal_to_goal(const ompl::base::Goal *a, const ompl::base::Goal *b) const {
    return (a->as<DroneEndEffectorNearTarget>()->getTarget() - b->as<DroneEndEffectorNearTarget>()->getTarget()).norm();
}

EuclideanOmplDistanceHeuristics::EuclideanOmplDistanceHeuristics(std::shared_ptr<DroneStateSpace> stateSpace)
        : state_space_(std::move(stateSpace)) {}

std::string EuclideanOmplDistanceHeuristics::name() const {
    return "Euclidean";
}

double GreatCircleOmplDistanceHeuristics::state_to_goal(const ompl::base::State *a, const ompl::base::Goal *b) const {
    moveit::core::RobotState sta(state_space_->getRobotModel());

    state_space_->copyToRobotState(sta, a);

    Eigen::Vector3d end_effector_pos = sta.getGlobalLinkTransform("end_effector").translation();

    return gcm.measure(end_effector_pos, b->as<DroneEndEffectorNearTarget>()->getTarget());
}

double GreatCircleOmplDistanceHeuristics::goal_to_goal(const ompl::base::Goal *a, const ompl::base::Goal *b) const {
    return gcm.measure(a->as<DroneEndEffectorNearTarget>()->getTarget(), b->as<DroneEndEffectorNearTarget>()->getTarget());
}

GreatCircleOmplDistanceHeuristics::GreatCircleOmplDistanceHeuristics(GreatCircleMetric gcm,
                                                                     std::shared_ptr<DroneStateSpace> stateSpace)
        : gcm(std::move(gcm)), state_space_(std::move(stateSpace)) {}

std::string GreatCircleOmplDistanceHeuristics::name() const {
    return "GreatCircle";
}