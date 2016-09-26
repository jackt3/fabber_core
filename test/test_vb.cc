//
// Tests specifically for the VB method

#include "newimage/newimageall.h"

#include "gtest/gtest.h"

#include "inference.h"
#include "inference_vb.h"
#include "dataset.h"
#include "setup.h"
#include "easylog.h"

namespace
{

// Hack to allow us to get at private/protected members
class PublicVersion: public VariationalBayesInferenceTechnique
{
public:
	using VariationalBayesInferenceTechnique::ImagePrior;
};

class VbTest: public ::testing::Test
{
protected:
	VbTest()
	{
		FabberSetup::SetupDefaults();
		EasyLog::StartLog(".", true);
	}

	virtual ~VbTest()
	{
		FabberSetup::Destroy();
		EasyLog::StopLog();
	}

	virtual void SetUp()
	{
		vb
				= reinterpret_cast<PublicVersion*> (static_cast<VariationalBayesInferenceTechnique*> (InferenceTechnique::NewFromName(
						"vb")));
		model = FwdModel::NewFromName("trivial");
	}

	virtual void TearDown()
	{
		delete vb;
	}

	void Run()
	{
		std::auto_ptr<FwdModel> fwd_model(FwdModel::NewFromName(
				rundata.GetString("model")));
		fwd_model->Initialize(rundata);

		vb->Initialize(fwd_model.get(), rundata);
		vb->DoCalculations(rundata);
		vb->SaveResults(rundata);
	}

	void Initialize()
	{
		rundata.SetVoxelCoords(voxelCoords);
		rundata.Set("noise", "white");
		vb->Initialize(model, rundata);
	}

	NEWMAT::Matrix voxelCoords;
	FabberRunData rundata;
	FwdModel *model;
	PublicVersion *vb;
};

// Test image priors. Note that this just
// checks the code works when they are specified
// not that they are actually having any effect!
TEST_F(VbTest, ImagePriors)
{
	int NTIMES = 10; // needs to be even
	int VSIZE = 5;
	float VAL = 7.32;

	// Create coordinates and data matrices
	NEWMAT::Matrix voxelCoords, data, iprior_data;
	data.ReSize(NTIMES, VSIZE * VSIZE * VSIZE);
	iprior_data.ReSize(1, VSIZE * VSIZE * VSIZE);
	voxelCoords.ReSize(3, VSIZE * VSIZE * VSIZE);
	int v = 1;
	for (int z = 0; z < VSIZE; z++)
	{
		for (int y = 0; y < VSIZE; y++)
		{
			for (int x = 0; x < VSIZE; x++)
			{
				voxelCoords(1, v) = x;
				voxelCoords(2, v) = y;
				voxelCoords(3, v) = z;
				for (int n = 0; n < NTIMES; n++)
				{
					if (n % 2 == 0)
						data(n + 1, v) = VAL;
					else
						data(n + 1, v) = VAL * 3;
				}
				iprior_data(1, v) = VAL * 1.5;
				v++;
			}
		}
	}
	rundata.SetVoxelCoords(voxelCoords);
	rundata.SetMainVoxelData(data);
	rundata.Set("noise", "white");
	rundata.Set("model", "trivial");
	rundata.Set("method", "vb");

	rundata.Set("PSP_byname1", "p");
	rundata.Set("PSP_byname1_type", "I");
	rundata.SetVoxelData("PSP_byname1_image", iprior_data);
	ASSERT_NO_THROW(Run());

	ASSERT_EQ(1, vb->ImagePrior.size());
	NEWMAT::RowVector iprior = vb->ImagePrior[0];
	ASSERT_EQ(VSIZE*VSIZE*VSIZE, iprior.Ncols());

	NEWMAT::Matrix mean = rundata.GetVoxelData("mean_p");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		ASSERT_FLOAT_EQ(mean(1, i+1), VAL*2);
		ASSERT_FLOAT_EQ(VAL*1.5, iprior(i+1));
	}
}

// Test image priors when stored in a file
TEST_F(VbTest, ImagePriorsFile)
{
	int NTIMES = 10; // needs to be even
	int VSIZE = 5;
	float VAL = 7.32;
	string FILENAME = "imageprior_data_temp";

	// Create coordinates and data matrices
	NEWMAT::Matrix voxelCoords, data, iprior_data;
	data.ReSize(NTIMES, VSIZE*VSIZE*VSIZE);
	iprior_data.ReSize(1, VSIZE*VSIZE*VSIZE);
	voxelCoords.ReSize(3, VSIZE*VSIZE*VSIZE);
	int v=1;
	for (int z = 0; z < VSIZE; z++)
	{
		for (int y = 0; y < VSIZE; y++)
		{
			for (int x = 0; x < VSIZE; x++)
			{
				voxelCoords(1, v) = x;
				voxelCoords(2, v) = y;
				voxelCoords(3, v) = z;
				for (int n = 0; n < NTIMES; n++)
				{
					if (n % 2 == 0) data(n + 1, v) = VAL;
					else data(n + 1, v) = VAL*3;
				}
				iprior_data(1, v) = VAL*1.5;
				v++;
			}
		}
	}

	// Save image prior data to a file
	NEWIMAGE::volume4D<float> data_out(VSIZE, VSIZE, VSIZE, 1);
	data_out.setmatrix(iprior_data);
	data_out.setDisplayMaximumMinimum(data_out.max(), data_out.min());
	save_volume4D(data_out, FILENAME);
	iprior_data.ReSize(1, 1);

	rundata.SetVoxelCoords(voxelCoords);
	rundata.SetMainVoxelData(data);
	rundata.Set("noise", "white");
	rundata.Set("model", "trivial");
	rundata.Set("method", "vb");

	rundata.Set("PSP_byname1", "p");
	rundata.Set("PSP_byname1_type", "I");
	rundata.Set("PSP_byname1_image", FILENAME);

	Run();

	ASSERT_EQ(1, vb->ImagePrior.size());
	NEWMAT::RowVector iprior = vb->ImagePrior[0];
	ASSERT_EQ(VSIZE*VSIZE*VSIZE, iprior.Ncols());

	NEWMAT::Matrix mean = rundata.GetVoxelData("mean_p");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		ASSERT_FLOAT_EQ(mean(1, i+1), VAL*2);
		ASSERT_FLOAT_EQ(VAL*1.5, iprior(i+1));
	}

	remove(FILENAME.c_str());
}

// Test restarting VB run
TEST_F(VbTest, Restart)
{
	int NTIMES = 10; // needs to be even
	int VSIZE = 5;
	float VAL = 7.32;
	int REPEATS = 50;
	int DEGREE = 5;

	// Create coordinates and data matrices
	// Data fitted to a quadratic function
	NEWMAT::Matrix voxelCoords, data;
	data.ReSize(NTIMES, VSIZE*VSIZE*VSIZE);
	voxelCoords.ReSize(3, VSIZE*VSIZE*VSIZE);
	int v=1;
	for (int z = 0; z < VSIZE; z++)
	{
		for (int y = 0; y < VSIZE; y++)
		{
			for (int x = 0; x < VSIZE; x++)
			{
				voxelCoords(1, v) = x;
				voxelCoords(2, v) = y;
				voxelCoords(3, v) = z;
				for (int n = 0; n < NTIMES; n++)
				{
					data(n + 1, v) = VAL+(1.5*VAL)*(n+1)*(n+1);
				}
				v++;
			}
		}
	}

	// Do just 1 iteration
	rundata.SetVoxelCoords(voxelCoords);
	rundata.SetMainVoxelData(data);
	rundata.Set("noise", "white");
	rundata.Set("model", "poly");
	rundata.Set("degree", stringify(DEGREE));
	rundata.Set("method", "vb");
	rundata.Set("max-iterations", "1");
	Run();

	// Make sure not converged after first iteration!
	NEWMAT::Matrix mean = rundata.GetVoxelData("mean_c0");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	ASSERT_NE(VAL, mean(1, 1));

	mean = rundata.GetVoxelData("mean_c2");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	ASSERT_NE(VAL*1.5, mean(1, 1));

	// Save results and repeat. Should converge even though we
	// only do 1 iteration each time.
	for (int repeat=0; repeat<REPEATS; repeat++) {
		NEWMAT::Matrix mvns = rundata.GetVoxelData("finalMVN");
		//ASSERT_EQ(mvns.Nrows(), 7);
		rundata.Set("max-iterations", "1");
		rundata.Set("continue-from-mvn", "mvns");
		rundata.SetVoxelData("continue-from-mvn", mvns);
		// This was just so you could see the convergence
		//mean = rundata.GetVoxelData("mean_c0");
		//cout << mean(1, 1) << " != " << VAL << endl;
		//mean = rundata.GetVoxelData("mean_c2");
		//cout << mean(1, 1) << " != " << VAL*1.5 << endl;

		TearDown();
		SetUp();
		Run();
	}

	mean = rundata.GetVoxelData("mean_c0");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		ASSERT_FLOAT_EQ(VAL, mean(1, i+1));
	}
	mean = rundata.GetVoxelData("mean_c1");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		// GTEST has difficulty with comparing floats to 0
		ASSERT_FLOAT_EQ(1, mean(1, i+1)+1);
	}
	mean = rundata.GetVoxelData("mean_c2");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		ASSERT_FLOAT_EQ(VAL*1.5, mean(1, i+1));
	}
}

// Test restarting VB run from a file
TEST_F(VbTest, RestartFromFile)
{
	int NTIMES = 10; // needs to be even
	int VSIZE = 5;
	float VAL = 7.32;
	int REPEATS = 50;
	int DEGREE = 5;
	string FILENAME = "temp_mvns";

	// Create coordinates and data matrices
	// Data fitted to a quadratic function
	NEWMAT::Matrix voxelCoords, data;
	data.ReSize(NTIMES, VSIZE*VSIZE*VSIZE);
	voxelCoords.ReSize(3, VSIZE*VSIZE*VSIZE);
	int v=1;
	for (int z = 0; z < VSIZE; z++)
	{
		for (int y = 0; y < VSIZE; y++)
		{
			for (int x = 0; x < VSIZE; x++)
			{
				voxelCoords(1, v) = x;
				voxelCoords(2, v) = y;
				voxelCoords(3, v) = z;
				for (int n = 0; n < NTIMES; n++)
				{
					data(n + 1, v) = VAL+(1.5*VAL)*(n+1)*(n+1);
				}
				v++;
			}
		}
	}

	// Do just 1 iteration
	rundata.SetVoxelCoords(voxelCoords);
	rundata.SetMainVoxelData(data);
	rundata.Set("noise", "white");
	rundata.Set("model", "poly");
	rundata.Set("degree", stringify(DEGREE));
	rundata.Set("method", "vb");
	rundata.Set("max-iterations", "1");
	Run();

	// Make sure not converged after first iteration!
	NEWMAT::Matrix mean = rundata.GetVoxelData("mean_c0");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	ASSERT_NE(VAL, mean(1, 1));

	mean = rundata.GetVoxelData("mean_c2");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	ASSERT_NE(VAL*1.5, mean(1, 1));

	// Save results and repeat. Should converge even though we
	// only do 1 iteration each time.
	for (int repeat=0; repeat<REPEATS; repeat++) {
		NEWMAT::Matrix mvns = rundata.GetVoxelData("finalMVN");
		NEWIMAGE::volume4D<float> data_out(VSIZE, VSIZE, VSIZE, mvns.Nrows());
		data_out.setmatrix(mvns);
		data_out.setDisplayMaximumMinimum(data_out.max(), data_out.min());
		save_volume4D(data_out, FILENAME);
		mvns.ReSize(1, 1);

		//ASSERT_EQ(mvns.Nrows(), 7);
		rundata.Set("max-iterations", "1");
		rundata.Set("continue-from-mvn", FILENAME);
		// This was just so you could see the convergence
		//mean = rundata.GetVoxelData("mean_c0");
		//cout << mean(1, 1) << " != " << VAL << endl;
		//mean = rundata.GetVoxelData("mean_c2");
		//cout << mean(1, 1) << " != " << VAL*1.5 << endl;

		TearDown();
		SetUp();
		Run();

		// Do this to stop picking up last run's data
		rundata.ClearVoxelData("continue-from-mvn");
		remove((FILENAME + ".nii.gz").c_str());
	}

	mean = rundata.GetVoxelData("mean_c0");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		ASSERT_FLOAT_EQ(VAL, mean(1, i+1));
	}
	mean = rundata.GetVoxelData("mean_c1");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		// GTEST has difficulty with comparing floats to 0
		ASSERT_FLOAT_EQ(1, mean(1, i+1)+1);
	}
	mean = rundata.GetVoxelData("mean_c2");
	ASSERT_EQ(mean.Nrows(), 1);
	ASSERT_EQ(mean.Ncols(), VSIZE*VSIZE*VSIZE);
	for (int i=0; i<VSIZE*VSIZE*VSIZE; i++)
	{
		ASSERT_FLOAT_EQ(VAL*1.5, mean(1, i+1));
	}
}

}
