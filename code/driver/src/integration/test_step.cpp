/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please 
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details 
 * regarding third party software licenses.
 */

#include "insieme/driver/integration/test_step.h"
#include <csignal>

#include <sstream>
#include <regex>

#include "insieme/utils/assert.h"
#include "insieme/utils/config.h"

#include<boost/tokenizer.hpp>


namespace insieme {
namespace driver {
namespace integration {

	namespace {

		namespace {

			TestResult runCommand(const TestSetup& setup, const string& cmd, const string& producedFile="") {

				vector<string> producedFiles;
				producedFiles.push_back(setup.stdOutFile);
				producedFiles.push_back(setup.stdErrFile);

				if(!producedFile.empty())
					producedFiles.push_back(producedFile);

				string outfile="";
				if(!setup.outputFile.empty()){
					producedFiles.push_back(setup.outputFile);
					outfile= " -o "+setup.outputFile;
				}

				// if it is a mock-run do nothing
				if (setup.mockRun) {
					return TestResult(true,0,0,"","",cmd + outfile);
				}

				string realCmd=string(TIME_COMMAND)+string(" -f \"\nTIME%e\nMEM%M\" ")+cmd + outfile +" >"+setup.stdOutFile+" 2>"+setup.stdErrFile;

				//TODO enable perf support
				//if(setup.enablePerf)

				//get return value, stdOut and stdErr
				int retVal=system(realCmd.c_str());

				//TODO change this to handle SIGINT signal
				if(retVal==512)
					exit(0);

			   if (WIFSIGNALED(retVal) &&
				   (WTERMSIG(retVal) == SIGINT || WTERMSIG(retVal) == SIGQUIT))
				   	   std::cout<<"killed"<<std::endl;

				string output=readFile(setup.stdOutFile);
				string error=readFile(setup.stdErrFile);

				float mem=0.0;
				float time=0.0;

				//get time and memory values and remove them from stdError
				string stdErr;
				boost::char_separator<char> sep("\n");
				boost::tokenizer<boost::char_separator<char>> tok(error,sep);
				for(boost::tokenizer<boost::char_separator<char>>::iterator beg=tok.begin(); beg!=tok.end();++beg){
					string token(*beg);
					if(token.find("TIME")==0)
						time=atof(token.substr(4).c_str());
					else if (token.find("MEM")==0)
						mem=atof(token.substr(3).c_str());
					else
						stdErr+=token+"\n";
				}


				if(retVal>0)
					return TestResult(false,time,mem,output,stdErr,cmd,producedFiles);

				return TestResult(true,time,mem,output,stdErr,cmd,producedFiles);
			}

			namespace fs = boost::filesystem;

			typedef std::set<std::string> Dependencies;

			enum Backend {
				Sequential, Runtime
			};

			enum Language {
				C, CPP
			};

			string getExtension(Language ext) {
				switch(ext) {
				case C:   return "c";
				case CPP: return "cpp";
				}
				return "xxx";
			}

			string getBackendKey(Backend be) {
				switch(be) {
				case Sequential:   	return "seq";
				case Runtime: 		return "run";
				}
				return "xxx";
			}


			//TODO MAKE FLAGS STEP SPECIFIC

			TestStep createRefCompStep(const string& name, Language l) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					auto props = test.getPropertiesFor(name);

					std::stringstream cmd;
					TestSetup set=setup;

					// start with executable
					cmd<<props["compiler"];

					// add include directories
					for(const auto& cur : test.getIncludeDirs()) {
						cmd << " -I" << cur.string();
					}

					// add external lib dirs
					for(const auto& cur : test.getLibDirs()) {
						cmd <<" -L"<<cur.string();
					}

					// add external libs
					for(const auto& cur : test.getLibNames()) {
						cmd <<" -l"<<cur;
					}

					// add input files
					for(const auto& cur : test.getFiles()) {
						cmd << " " << cur.string();
					}

					std::vector<string> flags=test.getCompilerArguments(name);
					// get all flags defined by properties
					for (string s: flags){
						cmd <<" "<<s;
					}

					//get definitions
					for_each(test.getDefinitions(name), [&](const std::pair<string,string>& def) {
						cmd<<"-D"<<def.first<<"="<<def.second<<" ";
					});

					// set output file, stdOutFile and stdErrFile
					set.outputFile=test.getDirectory().string()+"/"+test.getBaseName()+".ref";
					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".ref.comp.out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".ref.comp.err.out";

					// run it
					return runCommand(set, cmd.str());
				},std::set<std::string>(),COMPILE);
			}

			TestStep createRefRunStep(const string& name, const Dependencies& deps = Dependencies()) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					std::stringstream cmd;
					TestSetup set=setup;
					auto props = test.getPropertiesFor(name);

					// start with executable
					cmd << test.getDirectory().string() << "/" << test.getBaseName() << ".ref";

					// add arguments
					string exFlags=props["executionFlags"];

					// replace {path} with actual path
					while(exFlags.find("{PATH}")!=std::string::npos){
						exFlags.replace(exFlags.find("{PATH}"),6,test.getDirectory().string());
					}

					// replace {threads} with number of required threads
					// TODO change number to a good value (statistics)
					while(exFlags.find("{THREADS}")!=std::string::npos){
						exFlags.replace(exFlags.find("{THREADS}"),9,"12");
					}

					cmd <<" "<< exFlags;

					// set output files
					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".ref.out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".ref.err.out";

					// run it
					return runCommand(set, cmd.str());
				}, deps,RUN);
			}


			TestStep createMainSemaStep(const string& name, Language l, const Dependencies& deps = Dependencies()) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					auto props = test.getPropertiesFor(name);

					std::stringstream cmd;
					TestSetup set=setup;

					// start with executable
					cmd << props["compiler"];

					// enable semantic tests
					cmd << " -S";

					// also dump IR
					std::string irFile=test.getDirectory().string() + "/" + test.getName() + ".ir";
					cmd << " --dump-ir " << irFile;

					// add include directories
					for(const auto& cur : test.getIncludeDirs()) {
						cmd << " -I" << cur.string();
					}

					// add input files
					for(const auto& cur : test.getFiles()) {
						cmd << " " << cur.string();
					}

					std::vector<string> flags=test.getCompilerArguments(name);
					// get all flags defined by properties
					for (string s: flags){
						cmd <<" "<<s;
					}

					//get definitions
					for_each(test.getDefinitions(name), [&](const std::pair<string,string>& def) {
						cmd<<"-D"<<def.first<<"="<<def.second<<" ";
					});

					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".sema.comp.out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".sema.comp.err.out";

					// run it
					return runCommand(set, cmd.str(),irFile);
				}, deps,COMPILE);
			}

			TestStep createMainConversionStep(const string& name, Backend backend, Language l, const Dependencies& deps = Dependencies()) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					auto props = test.getPropertiesFor(name);

					std::stringstream cmd;
					TestSetup set=setup;

					// start with executable
					cmd << props["compiler"];

					// determine backend
					string be = getBackendKey(backend);
					cmd << " -b " << be;

					// add include directories
					for(const auto& cur : test.getIncludeDirs()) {
						cmd << " -I" << cur.string();
					}

					// add input files
					for(const auto& cur : test.getFiles()) {
						cmd << " " << cur.string();
					}

					std::vector<string> flags=test.getCompilerArguments(name);
					// get all flags defined by properties
					for (string s: flags){
						cmd <<" "<<s;
					}

					//get definitions
					for_each(test.getDefinitions(name), [&](const std::pair<string,string>& def) {
						cmd<<"-D"<<def.first<<"="<<def.second<<" ";
					});

					// set output file, stdOut file and stdErr file
					set.outputFile=test.getDirectory().string()+"/"+test.getBaseName()+".insieme."+be+"."+getExtension(l);
					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".conv.out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".conv.err.out";

					// run it
					return runCommand(set, cmd.str());
				}, deps,COMPILE);
			}

			TestStep createMainCompilationStep(const string& name, Backend backend, Language l, const Dependencies& deps = Dependencies()) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					auto props = test.getPropertiesFor(name);

					std::stringstream cmd;
					TestSetup set=setup;

					// start with executable
					cmd << props["compiler"];

					// determine backend
					string be = getBackendKey(backend);

					// add include directories
					for(const auto& cur : test.getIncludeDirs()) {
						cmd << " -I" << cur.string();
					}
					cmd << " -I "<< SRC_ROOT_DIR << "runtime/include";

					// add external lib dirs
					for(const auto& cur : test.getLibDirs()) {
						cmd <<" -L"<<cur.string();
					}

					// add external libs
					for(const auto& cur : test.getLibNames()) {
						cmd <<" -l"<<cur;
					}

					// add input file
					cmd << " " << test.getDirectory().string() << "/" << test.getBaseName() << ".insieme." << be << "." << getExtension(l);

					std::vector<string> flags=test.getCompilerArguments(name);
					// get all flags defined by properties
					for (string s: flags){
						cmd <<" "<<s;
					};

					//get definitions
					for_each(test.getDefinitions(name), [&](const std::pair<string,string>& def) {
						cmd<<"-D"<<def.first<<"="<<def.second<<" ";
					});

					// set output file, stdOut file and stdErr file
					set.outputFile=test.getDirectory().string()+"/"+test.getBaseName()+".insieme."+be;
					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".comp.out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".comp.err.out";

					// run it
					return runCommand(set, cmd.str());
				}, deps,COMPILE);
			}

			TestStep createMainExecuteStep(const string& name, Backend backend, const Dependencies& deps = Dependencies()) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					std::stringstream cmd;
					TestSetup set=setup;
					auto props = test.getPropertiesFor(name);

					// determine backend
					string be = getBackendKey(backend);

					// start with executable
					cmd << test.getDirectory().string() << "/" << test.getBaseName() << ".insieme." << be;

					// add arguments
					string exFlags=props["executionFlags"];

					// replace {path} with actual path
					while(exFlags.find("{PATH}")!=std::string::npos){
						exFlags.replace(exFlags.find("{PATH}"),6,test.getDirectory().string());
					}

					// replace {threads} with number of required threads
					// TODO change number to a good value (statistics)
					while(exFlags.find("{THREADS}")!=std::string::npos){
						exFlags.replace(exFlags.find("{THREADS}"),9,"12");
					}


					cmd <<" "<< exFlags;

					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".insieme."+be+".out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".insieme."+be+".err.out";

					// run it
					return runCommand(set, cmd.str());
				}, deps,RUN);
			}

			TestStep createMainCheckStep(const string& name, Backend backend, Language l, const Dependencies& deps = Dependencies()) {
				return TestStep(name, [=](const TestSetup& setup, const IntegrationTestCase& test)->TestResult {
					auto props = test.getPropertiesFor(name);

					std::stringstream cmd;
					TestSetup set=setup;

					// define comparison script
					cmd << props["sortdiff"];

					// determine backend
					string be = getBackendKey(backend);

					// start with executable
					cmd << " " << test.getDirectory().string() << "/" << test.getBaseName() << ".ref.out";

					// pipe result to output file
					cmd << " " << test.getDirectory().string() << "/" << test.getBaseName() << ".insieme." << be << ".out";

					// add awk pattern
					cmd << " "<< props["outputAwk"];

					set.stdOutFile=test.getDirectory().string()+"/"+test.getBaseName()+".match.out";
					set.stdErrFile=test.getDirectory().string()+"/"+test.getBaseName()+".match.err.out";

					// run it
					return runCommand(set, cmd.str());
				}, deps,CHECK);
			}


		}

		std::map<std::string,TestStep> createFullStepList() {

			std::map<std::string,TestStep> list;

			auto add = [&](const TestStep& step) {
				list.insert({step.getName(), step});
			};

			// --- real steps ----

			add(createRefCompStep("ref_c_compile", C));
			add(createRefCompStep("ref_c++_compile", CPP));

			add(createRefRunStep("ref_c_execute", { "ref_c_compile" }));
			add(createRefRunStep("ref_c++_execute", { "ref_c++_compile" }));

			add(createMainSemaStep("main_c_sema", C));
			add(createMainSemaStep("main_cxx_sema", CPP));

			add(createMainConversionStep("main_seq_convert", Sequential, C));
			add(createMainConversionStep("main_run_convert", Runtime, C));

			add(createMainConversionStep("main_seq_c++_convert", Sequential, CPP));
			add(createMainConversionStep("main_run_c++_convert", Runtime, CPP));

			add(createMainCompilationStep("main_seq_compile", Sequential, C, { "main_seq_convert" }));
			add(createMainCompilationStep("main_run_compile", Runtime, C, { "main_run_convert" }));

			add(createMainCompilationStep("main_seq_c++_compile", Sequential, CPP, { "main_seq_c++_convert" }));
			add(createMainCompilationStep("main_run_c++_compile", Runtime, CPP, { "main_run_c++_convert" }));

			add(createMainExecuteStep("main_seq_execute", Sequential, { "main_seq_compile" }));
			add(createMainExecuteStep("main_run_execute", Runtime, { "main_run_compile" }));

			add(createMainExecuteStep("main_seq_c++_execute", Sequential, { "main_seq_c++_compile" }));
			add(createMainExecuteStep("main_run_c++_execute", Runtime, { "main_run_c++_compile" }));

			add(createMainCheckStep("main_seq_check", Sequential, C, { "main_seq_execute", "ref_c_execute" }));
			add(createMainCheckStep("main_run_check", Runtime, C, { "main_run_execute", "ref_c_execute" }));

			add(createMainCheckStep("main_run_c++_check", Runtime, CPP, { "main_run_c++_execute", "ref_c++_execute" }));
			add(createMainCheckStep("main_seq_c++_check", Sequential, C, { "main_seq_c++_execute", "ref_c++_execute" }));


			return list;
		}

	}


	// a function obtaining an index of available steps
	const std::map<std::string,TestStep>& getFullStepList() {
		const static std::map<std::string,TestStep> list = createFullStepList();
		return list;
	}

	const TestStep& getStepByName(const std::string& name) {
		static const TestStep fail;

		const auto& list = getFullStepList();
		auto pos = list.find(name);
		if (pos != list.end()) {
			return pos->second;
		}
		assert_fail() << "Requested unknown step: " << name;
		return fail;
	}

	vector<TestStep> filterSteps(const vector<TestStep>& steps, const IntegrationTestCase& test) {
		auto props = test.getProperties();
		vector<TestStep> filteredSteps;

		for(const TestStep step:steps){
			string excludes=props["excludeSteps"];
			if(excludes.find(step.getName()) == std::string::npos)
				filteredSteps.push_back(step);
		}
		return filteredSteps;
	}

	namespace {

		void scheduleStep(const TestStep& step, vector<TestStep>& res) {

			// check whether test is already present
			if (contains(res, step)) return;

			// check that all dependencies are present
			for(const auto& cur : step.getDependencies()) {
				scheduleStep(getStepByName(cur), res);
			}

			// append step to schedule
			res.push_back(step);
		}

	}


	vector<TestStep> scheduleSteps(const vector<TestStep>& steps) {
		vector<TestStep> res;
		for(const auto& cur : steps) {
			scheduleStep(cur, res);
		}
		return res;
	}

	string readFile(string fileName){
					FILE* file=fopen(fileName.c_str(),"r");

					if(file==NULL)
						return string("");

					char buffer[1024];
					string output;

					while(!feof(file)){
						if(fgets(buffer,1024,file)!=NULL){
							output+=string(buffer);
						}
					}
					fclose(file);
					return output;
				}

} // end namespace integration
} // end namespace driver
} // end namespace insieme
