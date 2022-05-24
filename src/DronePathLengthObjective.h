
#ifndef NEW_PLANNERS_MOVEITPATHLENGTHOBJECTIVE_H
#define NEW_PLANNERS_MOVEITPATHLENGTHOBJECTIVE_H

#include <ompl/base/objectives/PathLengthOptimizationObjective.h>

class DronePathLengthObjective : public ompl::base::PathLengthOptimizationObjective {
public:
    DronePathLengthObjective(const ompl::base::SpaceInformationPtr &si);

    ompl::base::InformedSamplerPtr allocInformedStateSampler(
        const ompl::base::ProblemDefinitionPtr &probDefn,
        unsigned int maxNumberCalls) const override;

};


#endif //NEW_PLANNERS_MOVEITPATHLENGTHOBJECTIVE_H