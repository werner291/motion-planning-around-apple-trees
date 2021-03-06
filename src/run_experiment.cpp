#include "run_experiment.h"
#include "experiment_utils.h"
#include "probe_retreat_move.h"
#include "planners/MultiGoalPlanner.h"
#include "DistanceHeuristics.h"
#include "planners/ShellPathPlanner.h"
#include "planners/MultigoalPrmStar.h"
#include <range/v3/all.hpp>
#include <fstream>
#include <filesystem>
#include <boost/algorithm/string/predicate.hpp>

/**
 * A combination of a start state (with a number), a pick of apples, and a shared pointer to the scene to plan in.
 */
struct PlanningProblem {
	size_t start_state_id;
	ompl::base::ScopedState<> start_state;
	std::vector<Apple> apples;
	std::shared_ptr<AppleTreePlanningScene> scene_info;
};

/**
 * A combination of a planning problem, and an allocator to solve that problem.
 */
struct Run {
	NewMultiGoalPlannerAllocatorFn allocator;
	PlanningProblem problem;
};

std::vector<std::string> model_names_from_directory();

using namespace std;

/// Load a Moveit-based statespace with the drone.
std::shared_ptr<DroneStateSpace> loadStateSpace(const moveit::core::RobotModelConstPtr &model) {
	ompl_interface::ModelBasedStateSpaceSpecification spec(model, "whole_body");
	return make_shared<DroneStateSpace>(spec, TRANSLATION_BOUND);
}

/// Load the space information that represents the given scene,
/// including all the weird idiosyncracies of how we do planning.
ompl::base::SpaceInformationPtr
loadSpaceInformation(const std::shared_ptr<DroneStateSpace> &stateSpace,
					 const AppleTreePlanningScene &scene_info) {
	auto scene = setupPlanningScene(scene_info.scene_msg, stateSpace->getRobotModel());
	return initSpaceInformation(scene, scene->getRobotModel(), stateSpace);
}

/// Convert a PlanResult to JSON
Json::Value toJson(const MultiGoalPlanner::PlanResult &result) {
	Json::Value run_stats;
	run_stats["final_path_length"] = result.length();
	run_stats["goals_visited"] = (int) result.segments.size();
	return run_stats;
}

/// Generate a start state using OMPL types.
ompl::base::ScopedState<> genStartState(const shared_ptr<DroneStateSpace> &stateSpace, const int seed) {
	auto start_state_moveit = randomStateOutsideTree(stateSpace->getRobotModel(), seed);
	ompl::base::ScopedState<> start_state(stateSpace);
	stateSpace->copyToOMPLState(start_state.get(), start_state_moveit);
	return start_state;
}

/// Load a list of apple tree planning scenes from the directory.
std::vector<shared_ptr<AppleTreePlanningScene>> loadScenes() {

	std::vector<shared_ptr<AppleTreePlanningScene>> scenes;

	std::vector<string> model_names = {
			"appletree",
			"lemontree2",
			"orangetree4"
	};

	for (const auto &model_name: model_names) {

		// Load the scene.
		auto scene = createMeshBasedAppleTreePlanningSceneMessage(model_name);

		// Convert to a shared pointer and add to the list.
		scenes.push_back(make_shared<AppleTreePlanningScene>(scene));

	}

	return scenes;
}

vector<string> model_names_from_directory() {// Look into the 3d-models directory.
	// Note: due to copyright issues, you must decrypt the 3d-models.zip file
	// Ask the authors for the password or get your own apple tree models somewhere.
	string path = "3d-models";

	vector<string> model_names;

	for (const auto &entry: std::filesystem::directory_iterator(path)) {
		// If the file ends in "_trunk.dae" we identify it as an apple tree model
		// and assume the other files exist.
		if (boost::algorithm::ends_with(entry.path().filename().c_str(), "_trunk.dae")) {

			// Extract the filename from the path.
			auto filename = entry.path().filename().string();

			// Chop off the "_trunk.dae" suffix.
			auto model_name = filename.substr(0, filename.size() - string("_trunk.dae").size());
			model_names.push_back(model_name);
		}
	}
	return model_names;
}

/// Pick a subset of size at most napples uniformly at random.
/// If the size is larger than the number of apples, return all apples (they will still be shuffled).
/// Function requires an Rng to make seeding it possible.
template<typename Rng>
vector<Apple> random_subset(Rng &rng,
							const size_t napples,
							const std::shared_ptr<AppleTreePlanningScene> &scene_info,
							vector<Apple> apples) {

	// Grab a random subset of the apples of size napples.
	shuffle(apples.begin(), apples.end(), rng);

	// If the size is larger than the number of apples, return all apples.
	if (napples < apples.size()) {
		apples.resize(napples);
	}

	return apples;
}

/// Generate a set of planning problems based on the given numbers of apples, the set of scenes, and the number of runs.
template<typename Rng>
std::vector<PlanningProblem> genPlanningProblems(const int num_runs,
												 const vector<size_t> &napples,
												 const shared_ptr<DroneStateSpace> &stateSpace,
												 const vector<shared_ptr<AppleTreePlanningScene>> &scenes,
												 Rng &rng) {

	std::vector<PlanningProblem> problems;

	for (const auto& scene: scenes) {
		for (const auto& n: napples) {

			assert(n <= scene->apples.size());

			for (int i = 0; i < num_runs; i++) {
				auto start_state = genStartState(stateSpace, rng());
				auto apples = random_subset(rng, n, scene, scene->apples);

				problems.push_back( PlanningProblem {static_cast<size_t>(i), start_state, apples, scene } );
			}
		}
	}

	return problems;
}

/// Given a set of planner planner allocators and problems,
/// generate a list of Runs and shuffle them with the given Rng.
template<typename Rng>
vector<Run> genRuns(const vector<NewMultiGoalPlannerAllocatorFn> &allocators,
					const vector<PlanningProblem> &planning_problems,
					Rng &rng) {

	// Generate runs simply by taking the cartesian product of the allocators and the problems.
	vector<Run> tasks = ranges::views::cartesian_product(allocators, planning_problems) |
						ranges::views::transform([&](const auto &tuple) {
							return Run{std::get<0>(tuple), std::get<1>(tuple)};
						}) | ranges::to_vector;

	// Shuffle, but use the same seed every time.
	// The purpose of this shuffle is to reduce the effect of the order of the runs.
	// however, we don't really care about true randomness,
	// more about just making sure there's a good variety in the ordering.
	shuffle(tasks.begin(), tasks.end(), rng);

	return tasks;
}

/// The program crashes sometimes, so this reloads the already-gathered statistics from disk if available.
/// Runs a few simple validation steps to make sure we're not clobbering our stats.
void tryReloadAndValidateCachedStats(const string &results_path,
									 const vector<Run> &tasks,
									 Json::Value &statistics,
									 const size_t BATCH_SIZE) {

	// load if available.
	ifstream ifs(results_path);
	if (ifs.is_open()) {
		cout << "Loading previous run statistics from " << results_path << endl;
		ifs >> statistics;
	}

	// Stats are saved only if all tasks are completed, or a batch has finished.
	// Anything different from this is indicates something suspicious.
	if (!(statistics.size() == tasks.size() || statistics.size() % BATCH_SIZE == 0)) {
		throw runtime_error("Invalid previous run statistics (not a batch size)");
	}

	// Ensure that the task numbers line up to make sure the RNG hasn't been disturbed.
	for (size_t i = 0; i < statistics.size(); i++) {

		// If the program crashed mid-run, sometimes there will be gaps in the file.
		// This is not an issue, these will simply be run again.
		bool taskWasRun = statistics[(int) i].isMember("start_state");

		if (taskWasRun &&
			// Number should match up with the task list. We use an RNG with a constant seed,
			// so the order of the runs is deterministic, but it's still a good idea to check.
			statistics[(int) i]["start_state"].asInt() != tasks[i].problem.start_state_id) {
			throw runtime_error("Invalid previous run statistics (start state ID mismatch)");
		}
	}
}

/// Run a single planner-problem pair.
Json::Value run_task(const moveit::core::RobotModelConstPtr &drone, const Run &run) {
	const auto &[planner_allocator, start_state_pair] = run;
	const auto &[run_i, start_state, apples, scene] = start_state_pair;

	// Initialize the OMPL stuff. We do this separately for each run to prevent state cross-contamination.

	// *Somewhere* in the state space is something that isn't thread-safe despite const-ness.
	// So, we just re-create the state space every time just to be safe.
	auto threadLocalStateSpace = loadStateSpace(drone);

	// Collision-space is "thread-safe" by using locking. So, if we want to get any speedup at all,
	// we'll need to copy this every time.
	auto si = loadSpaceInformation(threadLocalStateSpace, *scene);

	// Allocate the planner. Again, we do this from scratch every time to prevent cross-contamination.
	auto planner = planner_allocator(*scene, si);

	// Construct the set of goals using OMPL types.
	auto goals = constructNewAppleGoals(si, apples);

	// Objective is path length minimization.
	auto objective = make_shared<DronePathLengthObjective>(si);

	auto timeout = ompl::base::timedPlannerTerminationCondition(std::chrono::minutes(5));

	MultiGoalPlanner::PlanResult result;

	// Run the planner, timing the runtime.
	auto start_time = ompl::time::now();
	try {
		result = planner->plan(si, start_state.get(), goals, *scene, timeout);
	} catch (PlanningTimeout &) {
		// If we timed out, we return an empty path.
		result.segments.clear();
		std::cout << "Planning timed out" << std::endl;
	}
	auto run_time = ompl::time::seconds(ompl::time::now() - start_time);

	// Construct the output JSON.
	auto plan_result = toJson(result);
	plan_result["run_time"] = run_time;
	plan_result["start_state"] = (int) run_i;
	plan_result["scene_name"] = scene->scene_msg.name;
	plan_result["napples"] = apples.size();
	plan_result["planner_params"] = planner->parameters();
	plan_result["planner_name"] = planner->name();

	return plan_result;
}

/// A bit of a hackish way to determine the task number that the worker should run next, if any.
/// Will increment the global_cursor every time it examines a task, such that other workers will not look at it again.
std::optional<size_t> next_task(size_t& global_cursor, std::mutex& cursor_mutex, Json::Value& statistics, const size_t ntasks) {
	// Prevent more than one thread from touching the cursor at once.
	std::lock_guard<std::mutex> lock(cursor_mutex);

	// Keep trying until we run out of tasks.
	while (global_cursor < ntasks) {

		// Grab the current task and bump the cursor.
		size_t thread_current_task = global_cursor++;

		// the stats already have an entry for this task, so we can skip it.
		if (statistics[(int) thread_current_task].isMember("start_state")) {
			std::cout << "Skipping task " << thread_current_task  << " because it was already completed." << std::endl;
			continue;
		}

		// Good to go.
		return {thread_current_task};
	}

	// We're out of tasks
	return {};
}

// Dump the statistics to disk.
void write_stats(const string &results_path, const Json::Value &statistics) {
	cout << "Saving statistics to " << results_path << endl;
	ofstream ofs(results_path);
	ofs << statistics;
}

/// Store the run result, and save it if we hit a multiple of the batch counter.
/// Locks the mutex so that this can be called by parallel workers.
void storePlanResult(const string &results_path,
					 const size_t BATCH_SIZE,
					 const optional<size_t> &thread_current_task,
					 const Json::Value &plan_result,
					 mutex &mut,
					 Json::Value &statistics) {

	lock_guard<mutex> lock(mut);

	statistics[(int) *thread_current_task] = plan_result;

	if (statistics.size() % BATCH_SIZE == 0) {
		write_stats(results_path, statistics);
	}
}

/// Run a set of planners on a set of problems, gathering statistics about them.
/// Process is checkpointed to prevent crashes from causing significant data loss.
void run_planner_experiment(const std::vector<NewMultiGoalPlannerAllocatorFn> &allocators,
							const std::string &results_path,
							const int num_runs,
							const std::vector<size_t> &napples,
							const unsigned int nworkers) {

	// Load the robot model.
	// It never changes, so it's safe to use in all workers.
	// Constify it just to be sure.
	const auto drone = std::const_pointer_cast<const moveit::core::RobotModel>(loadRobotModel());

	// Load the apple tree model with some metadata.
	vector<shared_ptr<AppleTreePlanningScene>> scenes = loadScenes();

	std::vector<std::thread> threads;
	std::mutex mut;

	// Constant seed so that we get the same batch between runs (in case of crashes)
	auto rng = std::mt19937(42); // NOLINT(cert-msc51-cpp)

	// Generate the list of planning problems to solve (this is deterministic thanks to seeding the Rng.
	const auto planning_problems = genPlanningProblems(num_runs, napples, loadStateSpace(drone), scenes, rng);

	// Generate the planner-problem pairs.
	const auto runs = genRuns(allocators, planning_problems, rng);

	// Cursor for the workers to keep track of the last task in the list that has not yet been looked at.
	size_t current_task = 0;

	// The variable we'll be gathering our statistics in.
	// Shall be a list, one entry for each run/task.
	Json::Value statistics;

	// Number of runs after which we should checkpoint.
	// The exact number doesn't matter much, but if it's very small we'll save very often,
	// and if it's larger we'll save less often (at greater risk of losing data).
	const size_t BATCH_SIZE = 20;

	// Load the statistics from disk if there are any left over from a previous, crashed run.
	// Validates to make sure that the deterministic nature of the "randomized" task list is working.
	tryReloadAndValidateCachedStats(results_path, runs, statistics, BATCH_SIZE);

	// Start the workers.
	for (size_t thread_id = 0; thread_id < nworkers; ++thread_id) {
		threads.emplace_back([&]() {

			// Keep going while tasks are available.
			while (auto thread_current_task = next_task(current_task, mut, statistics, runs.size())) {

				std::cout << "Starting task " << *thread_current_task << " of " << runs.size() << std::endl;

				// Run the run and gather stats about it.
				Json::Value plan_result = run_task(drone, runs[*thread_current_task]);

				cout << "Completed run " << *thread_current_task << " of " << runs.size() << endl;

				// Store the result.
				storePlanResult(results_path, BATCH_SIZE, thread_current_task, plan_result, mut, statistics);
			}
		});
	}

	// Wait for all threads to finish.
	for (auto &thread: threads) {
		thread.join();
	}

	std::cout << "All runs completed. " << std::endl;

	// Save the statistics to disk.
	write_stats(results_path, statistics);
}

/// Allocate a shared instance of og::PRM (because we need this as a function pointer).
ompl::base::PlannerPtr allocPRM(const ompl::base::SpaceInformationPtr &si) {
	return make_shared<ompl::geometric::PRM>(si);
}

std::shared_ptr<SphereShell> buildSphereShell(const AppleTreePlanningScene& scene) {
	auto enclosing = compute_enclosing_sphere(scene.scene_msg, 0.1);

	return std::make_shared<SphereShell>(enclosing.center, enclosing.radius);
}

/// Generate a list of ShellPathPlanner allocators to be run during an experiment.
std::vector<NewMultiGoalPlannerAllocatorFn> make_shellpath_allocators() {

	// We'll be taking a cartesian product of these.
	// Due to C++ ownership stupidity, it's best to keep plain old data here only.
//	bool applyShellstateOptimization[] = {true, false};
//	bool useImprovisedInformedSampler[] = {false, true};
//	bool tryLuckyShots[] = {false, true};
//	bool useCostConvergence[] = {false, true};
//	double ptp_time_seconds[] = {0.4, 0.5, 1.0};
	bool applyShellstateOptimization[] = {true};
	bool useImprovisedInformedSampler[] = {true};
	bool tryLuckyShots[] = {true};
	bool useCostConvergence[] = {true};
	double ptp_time_seconds[] = {0.4, 0.5, 1.0};

	// We explicitly use a function pointer here so we don't get burnt by this containing a reference to some local variable.
	ompl::base::PlannerPtr(*planner_allocators[])(const ompl::base::SpaceInformationPtr&) = { &allocPRM };

	// Generate all combinations.
	return ranges::views::cartesian_product(applyShellstateOptimization,
											ptp_time_seconds,
											planner_allocators,
											useImprovisedInformedSampler,
											tryLuckyShots,
											useCostConvergence) |
		   ranges::views::transform([](const auto tuple) -> NewMultiGoalPlannerAllocatorFn {

			   // Unpack the tuple.
			   auto [shellOptimize, ptp_budget, allocator, improvised_sampler, tryLucky, costConvergence] = tuple;

			   // Explicitly capture each by value to prevent any references from going out of scope.
			   return [shellOptimize = shellOptimize,
					   ptp_budget = ptp_budget,
					   allocator = allocator,
					   improvised_sampler = improvised_sampler,
					   tryLucky = tryLucky,
					   costConvergence = costConvergence](
					   const AppleTreePlanningScene &scene_info,
					   const ompl::base::SpaceInformationPtr &si) {

				   auto ptp = std::make_shared<SingleGoalPlannerMethods>(ptp_budget,
																		 si,
																		 std::make_shared<DronePathLengthObjective>(si),
																		 allocator,
																		 improvised_sampler,
																		 tryLucky,
																		 costConvergence);

				   return std::make_shared<ShellPathPlanner>(shellOptimize, ptp, buildSphereShell);
			   };
		   }) | ranges::to_vector; //  We return a vector to, again, prevent returning references to local variables.
}

std::vector<NewMultiGoalPlannerAllocatorFn> make_tsp_over_prm_allocators() {

	const auto samples_per_goal = ranges::views::iota(2, 10);
	const double plan_times_seconds[] = {1.0, 2.0, 5.0, 10.0, 15.0, 20.0};//, 30.0, 60.0};
	const bool optimize_segments_options[] = {true};//{false, true};

	return ranges::views::cartesian_product(plan_times_seconds, samples_per_goal, optimize_segments_options) |
		   ranges::views::transform([&](const auto tuple) -> NewMultiGoalPlannerAllocatorFn {

		auto [plan_time, samples, optimize_segments] = tuple;

		return [plan_time = plan_time, samples = samples, optimize_segments = optimize_segments](const AppleTreePlanningScene &scene_info,
																								 const ompl::base::SpaceInformationPtr &si) {
			return std::make_shared<MultigoalPrmStar>(plan_time, samples, optimize_segments);
		};
	}) | ranges::to_vector;
}


