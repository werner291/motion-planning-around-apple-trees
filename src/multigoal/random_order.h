//
// Created by werner on 30-09-21.
//

#ifndef NEW_PLANNERS_RANDOM_ORDER_H
#define NEW_PLANNERS_RANDOM_ORDER_H

#include "multi_goal_planners.h"

class RandomPlanner : public MultiGoalPlanner {

public:
    MultiGoalPlanResult plan(const TreeScene &apples, const moveit::core::RobotState &start_state,
                             const robowflex::SceneConstPtr &scene, const robowflex::RobotConstPtr &robot,
                             ompl::base::Planner &point_to_point_planner) override;

    std::string getName() override;
};

#endif //NEW_PLANNERS_RANDOM_ORDER_H
