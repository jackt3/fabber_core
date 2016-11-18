/*  FABBER - Fast ASL and BOLD Bayesian Estimation Routine

 Adrian Groves and Michael Chappell, FMRIB Image Analysis & IBME QuBIc groups

 Copyright (C) 2007-2015 University of Oxford  */

/*  CCOPYRIGHT */

#include "fabber_core.h"
#include "fabber_version.h"
#include "inference.h"
#include "fwdmodel.h"
#include "fabber_io_newimage.h"

#include "utils/tracer_plus.h"

#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <memory>
#include <vector>

using Utilities::Tracer_Plus;
using namespace std;

/**
 * Print usage information.
 */
static void Usage()
{
	cout << "\n\nUsage: fabber [--<option>|--<option>=<value> ...]" << endl << endl
			<< "Use -@ <file> to read additional arguments in command line form from a text file (DEPRECATED)." << endl
			<< "Use -f <file> to read options in option=value form" << endl << endl << "General options " << endl
			<< endl;

	vector<OptionSpec> options;
	FabberRunData::GetOptions(options);

	for (int i = 0; i < options.size(); i++)
	{
		cout << options[i] << endl;
	}
}

/**
 * Run the default command line program
 */
int execute(int argc, char** argv)
{
	bool gzLog = false;
	int ret = 1;

	try
	{
		// Create a new Fabber run
		FabberIoNewimage io;
		FabberRunData params(&io);
		PercentProgressCheck percent;
		params.SetProgressCheck(&percent);
		params.Parse(argc, argv);

		string load_models = params.GetStringDefault("loadmodels", "");
		if (load_models != "")
		{
			FwdModel::LoadFromDynamicLibrary(load_models);
		}

		// Print usage information if no arguments given, or
		// if --help specified
		if (params.GetBool("help") || argc == 1)
		{
			string model = params.GetStringDefault("model", "");
			string method = params.GetStringDefault("method", "");
			if (model != "")
			{
				FwdModel::UsageFromName(model, cout);
			}
			else if (method != "")
			{
				InferenceTechnique::UsageFromName(method, cout);
			}
			else
			{
				Usage();
			}

			return 0;
		}
		else if (params.GetBool("listmodels"))
		{
			vector<string> models = FwdModel::GetKnown();
			vector<string>::iterator iter;
			for (iter = models.begin(); iter != models.end(); iter++)
			{
				cout << *iter << endl;
			}

			return 0;
		}
		else if (params.GetBool("listmethods"))
		{
			vector<string> infers = InferenceTechnique::GetKnown();
			vector<string>::iterator iter;
			for (iter = infers.begin(); iter != infers.end(); iter++)
			{
				cout << *iter << endl;
			}

			return 0;
		}

		cout << "----------------------" << endl;
		cout << "Welcome to FABBER v" << FabberRunData::GetVersion() << endl;
		cout << "----------------------" << endl;

		EasyLog::StartLog(params.GetStringDefault("output", "."), params.GetBool("overwrite"),
				params.GetBool("link-to-latest"));
		cout << "Logfile started: " << EasyLog::GetOutputDirectory() << "/logfile" << endl;

		// FIXME this is a hack but seems to be expected that the command line
		// tool will output parameter names to a file. Really should be an option!
		ofstream paramFile((EasyLog::GetOutputDirectory() + "/paramnames.txt").c_str());
		vector<string> paramNames;
		std::auto_ptr<FwdModel> fwd_model(FwdModel::NewFromName(params.GetString("model")));
		fwd_model->Initialize(params);
		fwd_model->NameParams(paramNames);
		for (unsigned i = 0; i < paramNames.size(); i++)
		{
			paramFile << paramNames[i] << endl;
		}
		paramFile.close();

		// Start timing/tracing if requested
		bool recordTimings = false;

		if (params.GetBool("debug-timings"))
		{
			recordTimings = true;
			Tracer_Plus::settimingon();
		}
		if (params.GetBool("debug-instant-stack"))
		{
			Tracer_Plus::setinstantstackon();
		} // instant stack isn't used?
		if (params.GetBool("debug-running-stack"))
		{
			Tracer_Plus::setrunningstackon();
		}

		// can't start it before this or it segfaults if an exception is thown with --debug-timings on.
		Tracer_Plus tr("FABBER main (outer)");

		// Start a new tracer for timing purposes
		{
			params.Run();
		}

		if (recordTimings)
		{
			tr.dump_times(EasyLog::GetOutputDirectory());
			LOG << "Timing profile information recorded to " << EasyLog::GetOutputDirectory() << "/timings.html"
					<< endl;
		}

		Warning::ReissueAll();

		// Only Gzip the logfile if we exit normally
		gzLog = params.GetBool("gzip-log");
		ret = 0;

	} catch (const DataNotFound& e)
	{
		Warning::ReissueAll();
		LOG_ERR("Data not found:\n  " << e.what() << endl);
		cerr << "Data not found:\n  " << e.what() << endl;
	} catch (const Invalid_option& e)
	{
		Warning::ReissueAll();
		LOG_ERR("Invalid_option exception caught in fabber:\n  " << e.what() << endl);
		cerr << "Invalid_option exception caught in fabber:\n  " << e.what() << endl;
	} catch (const exception& e)
	{
		Warning::ReissueAll();
		LOG_ERR("STL exception caught in fabber:\n  " << e.what() << endl);
		cerr << "STL exception caught in fabber:\n  " << e.what() << endl;
	} catch (NEWMAT::Exception& e)
	{
		Warning::ReissueAll();
		LOG_ERR("NEWMAT exception caught in fabber:\n  " << e.what() << endl);
		cerr << "NEWMAT exception caught in fabber:\n  " << e.what() << endl;
	} catch (...)
	{
		Warning::ReissueAll();
		LOG_ERR("Some other exception caught in fabber!" << endl);
		cerr << "Some other exception caught in fabber!" << endl;
	}

	if (EasyLog::LogStarted())
	{
		cout << endl << "Final logfile: " << EasyLog::GetOutputDirectory() << (gzLog ? "/logfile.gz" : "/logfile")
				<< endl;
		EasyLog::StopLog(gzLog);
	}
	else
	{
		// Flush any errors to stdout as we didn't get as far as starting the logfile
		EasyLog::StartLog(cout);
		EasyLog::StopLog();
	}
	return ret;
}
