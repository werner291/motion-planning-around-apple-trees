#ifndef NEW_PLANNERS_EXPERIMENTVISUALTOOLS_H
#define NEW_PLANNERS_EXPERIMENTVISUALTOOLS_H

#include <ros/node_handle.h>
#include <ompl/geometric/PathGeometric.h>
#include <moveit/robot_trajectory/robot_trajectory.h>
#include "procedural_tree_generation.h"

/**
 * A few helper functions for visualizing the results of an experiment in RViz.
 *
 * All topics published by this class are kept alive at least until the destructor is called.
 */
class ExperimentVisualTools {

	// Reference to this program's ROS node handle.
	ros::NodeHandle nh;

	// References to publishers so they don't go out-of-scope.
	std::vector<ros::Publisher> publisher_handles;

public:
	ExperimentVisualTools();

	/**
	 * Makes a PlanningScene message available on ROS topic "/planning_scene".
	 */
	void publishPlanningScene(const moveit_msgs::PlanningScene &scene_msg);

	/**
	 * For a set of apple-approach-path pairs, publishes a MarkerArray topic that visualizes the end-effector
	 * position at the start of the path connected with the target postion, to give an idea of where the path
	 * starts relative to the target without visualizing the full path (and causing visual clutter).
	 *
	 * @param apples 		The vector of apple-approach-path pairs.
	 * @param topic_name 	The ROS topic name.
	 */
	void dumpProjections(const std::vector<std::pair<Apple, ompl::geometric::PathGeometric>> &apples,
						 const std::string &topic_name);

	/**
	 * Publishes the given vector of paths as a single DisplayTrajectory message by concatenating them.
	 *
	 * @param si 			SpaceInformation to help with conversion to ROS messages (through Moveit).
	 * @param approaches 	The vector of apple-approach-path pairs.
	 * @param topic_name 	The ROS topic name.
	 */
	void dumpApproaches(const std::shared_ptr<ompl::base::SpaceInformation> &si,
						const std::vector<std::pair<Apple, ompl::geometric::PathGeometric>> &approaches,
						const std::string &topic_name);

	/**
	 * Publish the given path on a DisplayTrajectory topic, converting automatically from OMPL.
	 *
	 * @param si 			SpaceInformation to help with conversion to ROS messages (through Moveit).
	 * @param topic_name 	The ROS topic name.
	 * @param combined_path The path to publish,
	 */
	void publishPath(const std::shared_ptr<ompl::base::SpaceInformation> &si,
					 const std::string &topic_name,
					 const ompl::geometric::PathGeometric &combined_path);

	/**
	 * Publish the given trajectory on a DisplayTrajectory topic.
	 *
	 * @param topic_name
	 * @param moveit_trajectory
	 */
	void publishTrajectory(const std::string &topic_name, const robot_trajectory::RobotTrajectory &moveit_trajectory);
};

#endif //NEW_PLANNERS_EXPERIMENTVISUALTOOLS_H
