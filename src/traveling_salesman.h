
#ifndef NEW_PLANNERS_TRAVELING_SALESMAN_H
#define NEW_PLANNERS_TRAVELING_SALESMAN_H

#include <ortools/constraint_solver/routing.h>
#include <ortools/constraint_solver/routing_index_manager.h>
#include <ortools/constraint_solver/routing_parameters.h>
#include <boost/range/irange.hpp>
#include <utility>
#include "procedural_tree_generation.h"
#include "greatcircle.h"

class DistanceHeuristics {
public:
    [[nodiscard]] virtual std::string name() = 0;
    [[nodiscard]] virtual double start_to_apple(const Apple &) const = 0;
    [[nodiscard]] virtual double apple_to_apple(const Apple &, const Apple &) const = 0;
};

class EuclideanDistanceHeuristics : public DistanceHeuristics {

    Eigen::Vector3d start_end_effector_pos;

public:
    explicit EuclideanDistanceHeuristics(Eigen::Vector3d startEndEffectorPos);

    std::string name() override;

    [[nodiscard]] double start_to_apple(const Apple &apple) const override;

    [[nodiscard]] double apple_to_apple(const Apple &apple_a, const Apple &apple_b) const override;
};

class GreatcircleDistanceHeuristics : public DistanceHeuristics {

private:
    Eigen::Vector3d start_end_effector_pos;
    GreatCircleMetric gcm;

public:
    GreatcircleDistanceHeuristics(Eigen::Vector3d startEndEffectorPos, GreatCircleMetric gcm);

    std::string name() override;

    [[nodiscard]] double start_to_apple(const Apple &apple) const override;

    [[nodiscard]] double apple_to_apple(const Apple &apple_a, const Apple &apple_b) const override;
};

class OrderingStrategy {
public:
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::vector<size_t> apple_ordering(const std::vector<Apple> &apples, const DistanceHeuristics& distance) const = 0;
};

class GreedyOrderingStrategy : public OrderingStrategy {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] std::vector<size_t> apple_ordering(const std::vector<Apple> &apples, const DistanceHeuristics &distance) const override;

};

class ORToolsOrderingStrategy : public OrderingStrategy {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] std::vector<size_t> apple_ordering(const std::vector<Apple> &apples, const DistanceHeuristics &distance) const override;

};

double ordering_heuristic_cost(const std::vector<size_t>& ordering,
                               const std::vector<Apple>& apples,
                               const DistanceHeuristics& dh);

#endif //NEW_PLANNERS_TRAVELING_SALESMAN_H