// Tests for convergence detectors

#include "gtest/gtest.h"

#include "convergence.h"
#include "easylog.h"

namespace
{
class ConvergenceTest : public ::testing::Test
{
protected:
    ConvergenceTest()
        : rundata(0)
    {
        rundata.SetLogger(&log);
    }

    virtual ~ConvergenceTest()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }

    FabberRunData rundata;
    EasyLog log;
};

TEST_F(ConvergenceTest, TestCounting)
{
    int MAXITERS = 37;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("maxits");
    c->Initialize(rundata);

    ASSERT_EQ(false, c->UseF());
    for (int i = 0; i < MAXITERS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F));
    }
    ASSERT_EQ(true, c->Test(F));

    c->Reset();
    for (int i = 0; i < MAXITERS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F));
    }
    ASSERT_EQ(true, c->Test(F));
}

TEST_F(ConvergenceTest, TestFchangeConvergenceDetectorMaxIters)
{
    int MAXITERS = 37;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("pointzeroone");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());
    for (int i = 0; i < MAXITERS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F + 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F + 2 * MAXITERS * FCHANGE));

    c->Reset();
    for (int i = 0; i < MAXITERS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F + 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F + 2 * MAXITERS * FCHANGE));
}

TEST_F(ConvergenceTest, TestFchangeConvergenceDetectorChange)
{
    int MAXITERS = 37;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("pointzeroone");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());

    ASSERT_EQ(false, c->Test(F));

    // Increase
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));
    // Decrease
    ASSERT_EQ(false, c->Test(F));
    // Change must be less however with floats it's
    // difficult to get it really exactly equal
    ASSERT_EQ(false, c->Test(F + 1.01 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 1.99 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 1.99 * FCHANGE));

    c->Reset();
    ASSERT_EQ(false, c->Test(F + 1.99 * FCHANGE));
    ASSERT_EQ(false, c->Test(F));
    ASSERT_EQ(true, c->Test(F));
}

TEST_F(ConvergenceTest, TestFreduceConvergenceDetectorMaxIters)
{
    int MAXITERS = 37;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("freduce");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());
    for (int i = 0; i < MAXITERS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F + 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F + 2 * MAXITERS * FCHANGE));

    c->Reset();
    for (int i = 0; i < MAXITERS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F + 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F + 2 * MAXITERS * FCHANGE));
}

TEST_F(ConvergenceTest, TestFreduceConvergenceDetectorChange)
{
    int MAXITERS = 37;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("freduce");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());

    ASSERT_EQ(false, c->Test(F));

    // Increase
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));

    // Change must be less however with floats it's
    // difficult to get it really exactly equal
    ASSERT_EQ(false, c->Test(F + 3.01 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 3.99 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 3.99 * FCHANGE));

    c->Reset();
    ASSERT_EQ(false, c->Test(F + 3.99 * FCHANGE));
    ASSERT_EQ(false, c->Test(F + 5 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 5 * FCHANGE));
}

TEST_F(ConvergenceTest, TestFreduceConvergenceDetectorReduce)
{
    int MAXITERS = 37;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("freduce");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());

    ASSERT_EQ(false, c->Test(F));

    // Increase
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));
    ASSERT_EQ(true, c->Test(F - 2 * FCHANGE));

    c->Reset();
    ASSERT_EQ(false, c->Test(F - 3 * FCHANGE));
    ASSERT_EQ(false, c->Test(F));
    ASSERT_EQ(true, c->Test(F - 5 * FCHANGE));
}

TEST_F(ConvergenceTest, TestTrialModeConvergenceDetectorMaxIters)
{
    int MAXITERS = 37;
    int MAXTRIALS = 3;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    rundata.Set("max-trials", MAXTRIALS);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("trialmode");
    c->Initialize(rundata);

    // NOTE THAT THE TRIAL MODE CONVERGENCE DETECTOR GIVES YOU
    // 1 MORE ITERATION THAT REQUESTED, WE ARE KEEPING THIS
    // BEHAVIOUR FOR NOW FOR BACKWARDS COMPATIBILITY
    //
    // SHOULD REALLY BE MAXITERS-1 IN THE LOOPS BELOW
    ASSERT_EQ(true, c->UseF());
    for (int i = 0; i < MAXITERS; i++)
    {
        ASSERT_EQ(false, c->Test(F + 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F + 2 * MAXITERS * FCHANGE));

    c->Reset();
    for (int i = 0; i < MAXITERS; i++)
    {
        ASSERT_EQ(false, c->Test(F + 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F + 2 * MAXITERS * FCHANGE));
}

TEST_F(ConvergenceTest, TestTrialModeConvergenceDetectorChange)
{
    int MAXITERS = 37;
    int MAXTRIALS = 3;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    rundata.Set("max-trials", MAXTRIALS);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("trialmode");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());

    ASSERT_EQ(false, c->Test(F));

    // Always increase F because we're not testing the
    // special behaviour when F reduces here
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));

    // Change must be less than max, however with floats it's
    // difficult to get it really exactly equal
    ASSERT_EQ(false, c->Test(F + 3.01 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 3.99 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 3.99 * FCHANGE));

    c->Reset();
    ASSERT_EQ(false, c->Test(F + 3.99 * FCHANGE));
    ASSERT_EQ(false, c->Test(F + 5 * FCHANGE));
    ASSERT_EQ(true, c->Test(F + 5 * FCHANGE));
}

TEST_F(ConvergenceTest, TestTrialModeConvergenceDetectorReduce)
{
    int MAXITERS = 37;
    int MAXTRIALS = 3;
    double FCHANGE = 0.0001;
    double F = 12.1;

    rundata.Set("max-iterations", MAXITERS);
    rundata.Set("min-fchange", FCHANGE);
    rundata.Set("max-trials", MAXTRIALS);
    ConvergenceDetector *c = ConvergenceDetector::NewFromName("trialmode");
    c->Initialize(rundata);

    ASSERT_EQ(true, c->UseF());

    ASSERT_EQ(false, c->Test(F));

    // Increase
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));

    // Decreases, always by more than FCHANGE
    for (int i = 0; i < MAXTRIALS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F - 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F - 2 * MAXTRIALS * FCHANGE));

    c->Reset();
    ASSERT_EQ(false, c->Test(F));

    // Increase
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));
    // Decrease
    ASSERT_EQ(false, c->Test(F));
    // Increase again, should reset number of trials!
    ASSERT_EQ(false, c->Test(F + 2 * FCHANGE));

    // Decreases, always by more than FCHANGE
    for (int i = 0; i < MAXTRIALS - 1; i++)
    {
        ASSERT_EQ(false, c->Test(F - 2 * i * FCHANGE));
    }
    ASSERT_EQ(true, c->Test(F - 2 * MAXTRIALS * FCHANGE));
}
}
