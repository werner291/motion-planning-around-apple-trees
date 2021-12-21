
#include <gtest/gtest.h>
#include <ompl/base/spaces/SE3StateSpace.h>

// Run all the tests that were declared with TEST()
int main(int argc, char **argv){

    auto state = new ompl::base::SE3StateSpace();

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}