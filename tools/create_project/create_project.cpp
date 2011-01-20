/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $URL$
 * $Id$
 *
 */

#include "create_project.h"
#include "codeblocks.h"

#include "msvc.h"
#include "visualstudio.h"
#include "msbuild.h"

#include <fstream>
#include <iostream>

#include <stack>
#include <algorithm>
#include <iomanip>

#include <cstring>
#include <cstdlib>
#include <ctime>

#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
#include <sstream>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

namespace {
/**
 * Converts the given path to only use slashes as
 * delimiters.
 * This means that for example the path:
 *  foo/bar\test.txt
 * will be converted to:
 *  foo/bar/test.txt
 *
 * @param path Path string.
 * @return Converted path.
 */
std::string unifyPath(const std::string &path);

/**
 * Returns the last path component.
 *
 * @param path Path string.
 * @return Last path component.
 */
std::string getLastPathComponent(const std::string &path);

/**
 * Display the help text for the program.
 *
 * @param exe Name of the executable.
 */
void displayHelp(const char *exe);

/**
 * Structure for describing an FSNode. This is a very minimalistic
 * description, which includes everything we need.
 * It only contains the name of the node and whether it is a directory
 * or not.
 */
struct FSNode {
	FSNode() : name(), isDirectory(false) {}
	FSNode(const std::string &n, bool iD) : name(n), isDirectory(iD) {}

	std::string name; ///< Name of the file system node
	bool isDirectory; ///< Whether it is a directory or not
};

typedef std::list<FSNode> FileList;
} // End of anonymous namespace

enum ProjectType {
	kProjectNone,
	kProjectCodeBlocks,
	kProjectMSVC
};

int main(int argc, char *argv[]) {
#if !(defined(_WIN32) || defined(WIN32))
	// Initialize random number generator for UUID creation
	std::srand(std::time(0));
#endif

	if (argc < 2) {
		displayHelp(argv[0]);
		return -1;
	}

	const std::string srcDir = argv[1];

	BuildSetup setup;
	setup.srcDir = unifyPath(srcDir);

	if (setup.srcDir.at(setup.srcDir.size() - 1) == '/')
		setup.srcDir.erase(setup.srcDir.size() - 1);

	setup.filePrefix = setup.srcDir;
	setup.outputDir = '.';

	setup.engines = parseConfigure(setup.srcDir);

	if (setup.engines.empty()) {
		std::cout << "WARNING: No engines found in configure file or configure file missing in \"" << setup.srcDir << "\"\n";
		return 0;
	}

	setup.features = getAllFeatures();

	ProjectType projectType = kProjectNone;
	int msvcVersion = 9;

	// Parse command line arguments
	using std::cout;
	for (int i = 2; i < argc; ++i) {
		if (!std::strcmp(argv[i], "--list-engines")) {
			cout << " The following enables are available in the ScummVM source distribution\n"
			        " located at \"" << srcDir << "\":\n";

			cout << "   state  |       name      |     description\n\n";
			cout.setf(std::ios_base::left, std::ios_base::adjustfield);
			for (EngineDescList::const_iterator j = setup.engines.begin(); j != setup.engines.end(); ++j)
				cout << ' ' << (j->enable ? " enabled" : "disabled") << " | " << std::setw((std::streamsize)15) << j->name << std::setw((std::streamsize)0) << " | " << j->desc << "\n";
			cout.setf(std::ios_base::right, std::ios_base::adjustfield);

			return 0;

		} else if (!std::strcmp(argv[i], "--codeblocks")) {
			if (projectType != kProjectNone) {
				std::cerr << "ERROR: You cannot pass more than one project type!\n";
				return -1;
			}

			projectType = kProjectCodeBlocks;

		} else if (!std::strcmp(argv[i], "--msvc")) {
			if (projectType != kProjectNone) {
				std::cerr << "ERROR: You cannot pass more than one project type!\n";
				return -1;
			}

			projectType = kProjectMSVC;

		} else if (!std::strcmp(argv[i], "--msvc-version")) {
			if (i + 1 >= argc) {
				std::cerr << "ERROR: Missing \"version\" parameter for \"--msvc-version\"!\n";
				return -1;
			}

			msvcVersion = atoi(argv[++i]);

			if (msvcVersion != 8 && msvcVersion != 9 && msvcVersion != 10) {
				std::cerr << "ERROR: Unsupported version: \"" << msvcVersion << "\" passed to \"--msvc-version\"!\n";
				return -1;
			}
		} else if (!strncmp(argv[i], "--enable-", 9)) {
			const char *name = &argv[i][9];
			if (!*name) {
				std::cerr << "ERROR: Invalid command \"" << argv[i] << "\"\n";
				return -1;
			}

			if (!std::strcmp(name, "all-engines")) {
				for (EngineDescList::iterator j = setup.engines.begin(); j != setup.engines.end(); ++j)
					j->enable = true;
			} else if (!setEngineBuildState(name, setup.engines, true)) {
				// If none found, we'll try the features list
				if (!setFeatureBuildState(name, setup.features, true)) {
					std::cerr << "ERROR: \"" << name << "\" is neither an engine nor a feature!\n";
					return -1;
				}
			}
		} else if (!strncmp(argv[i], "--disable-", 10)) {
			const char *name = &argv[i][10];
			if (!*name) {
				std::cerr << "ERROR: Invalid command \"" << argv[i] << "\"\n";
				return -1;
			}

			if (!std::strcmp(name, "all-engines")) {
				for (EngineDescList::iterator j = setup.engines.begin(); j != setup.engines.end(); ++j)
					j->enable = false;
			} else if (!setEngineBuildState(name, setup.engines, false)) {
				// If none found, we'll try the features list
				if (!setFeatureBuildState(name, setup.features, false)) {
					std::cerr << "ERROR: \"" << name << "\" is neither an engine nor a feature!\n";
					return -1;
				}
			}
		} else if (!std::strcmp(argv[i], "--file-prefix")) {
			if (i + 1 >= argc) {
				std::cerr << "ERROR: Missing \"prefix\" parameter for \"--file-prefix\"!\n";
				return -1;
			}

			setup.filePrefix = unifyPath(argv[++i]);
			if (setup.filePrefix.at(setup.filePrefix.size() - 1) == '/')
				setup.filePrefix.erase(setup.filePrefix.size() - 1);
		} else if (!std::strcmp(argv[i], "--output-dir")) {
			if (i + 1 >= argc) {
				std::cerr << "ERROR: Missing \"path\" parameter for \"--output-dirx\"!\n";
				return -1;
			}

			setup.outputDir = unifyPath(argv[++i]);
			if (setup.outputDir.at(setup.outputDir.size() - 1) == '/')
				setup.outputDir.erase(setup.outputDir.size() - 1);

		} else if (!std::strcmp(argv[i], "--build-events")) {
			setup.runBuildEvents = true;
		} else {
			std::cerr << "ERROR: Unknown parameter \"" << argv[i] << "\"\n";
			return -1;
		}
	}

	// Print status
	cout << "Enabled engines:\n\n";
	for (EngineDescList::const_iterator i = setup.engines.begin(); i != setup.engines.end(); ++i) {
		if (i->enable)
			cout << "    " << i->desc << '\n';
	}

	cout << "\nDisabled engines:\n\n";
	for (EngineDescList::const_iterator i = setup.engines.begin(); i != setup.engines.end(); ++i) {
		if (!i->enable)
			cout << "    " << i->desc << '\n';
	}

	cout << "\nEnabled features:\n\n";
	for (FeatureList::const_iterator i = setup.features.begin(); i != setup.features.end(); ++i) {
		if (i->enable)
			cout << "    " << i->description << '\n';
	}

	cout << "\nDisabled features:\n\n";
	for (FeatureList::const_iterator i = setup.features.begin(); i != setup.features.end(); ++i) {
		if (!i->enable)
			cout << "    " << i->description << '\n';
	}

	// Setup defines and libraries
	setup.defines = getEngineDefines(setup.engines);
	setup.libraries = getFeatureLibraries(setup.features);

	// Add features
	StringList featureDefines = getFeatureDefines(setup.features);
	setup.defines.splice(setup.defines.begin(), featureDefines);

	// Windows only has support for the SDL backend, so we hardcode it here (along with winmm)
	setup.defines.push_back("WIN32");
	setup.defines.push_back("SDL_BACKEND");
	setup.libraries.push_back("sdl");
	setup.libraries.push_back("winmm");

// Initialize global & project-specific warnings
#define SET_GLOBAL_WARNINGS(...) \
	{ \
		std::string global[PP_NARG(__VA_ARGS__)] = { __VA_ARGS__ }; \
		globalWarnings.assign(global, global + (sizeof(global) / sizeof(global[0]))); \
	}

#define SET_WARNINGS(name, ...) \
	{ \
		std::string project[PP_NARG(__VA_ARGS__)] = { __VA_ARGS__ }; \
		projectWarnings[name].assign(project, project + (sizeof(project) / sizeof(project[0]))); \
	}

	// List of global warnings and map of project-specific warnings
	StringList globalWarnings;
	std::map<std::string, StringList> projectWarnings;

	CreateProjectTool::ProjectProvider *provider = NULL;

	switch (projectType) {
	default:
	case kProjectNone:
		std::cerr << "ERROR: No project type has been specified!\n";
		return -1;

	case kProjectCodeBlocks:
		////////////////////////////////////////////////////////////////////////////
		// Code::Blocks is using GCC behind the scenes, so we need to pass a list
		// of options to enable or disable warnings
		////////////////////////////////////////////////////////////////////////////
		//
		// -Wall
		//   enable all warnings
		//
		// -Wno-long-long -Wno-multichar -Wno-unknown-pragmas -Wno-reorder
		//   disable annoying and not-so-useful warnings
		//
		// -Wpointer-arith -Wcast-qual -Wcast-align
		// -Wshadow -Wimplicit -Wnon-virtual-dtor -Wwrite-strings
		//   enable even more warnings...
		//
		// -fno-rtti -fno-exceptions -fcheck-new
		//   disable RTTI and exceptions, and enable checking of pointers returned
		//   by "new"
		//
		////////////////////////////////////////////////////////////////////////////

		SET_GLOBAL_WARNINGS("-Wall", "-Wno-long-long", "-Wno-multichar", "-Wno-unknown-pragmas", "-Wno-reorder",
		                    "-Wpointer-arith", "-Wcast-qual", "-Wcast-align", "-Wshadow", "-Wimplicit",
		                    "-Wnon-virtual-dtor", "-Wwrite-strings", "-fno-rtti", "-fno-exceptions", "-fcheck-new");

		provider = new CreateProjectTool::CodeBlocksProvider(globalWarnings, projectWarnings);

		break;

	case kProjectMSVC:
		////////////////////////////////////////////////////////////////////////////
		// For Visual Studio, all warnings are on by default in the project files,
		// so we pass a list of warnings to disable globally or per-project
		//
		// Tracker reference:
		// https://sourceforge.net/tracker/?func=detail&aid=2909981&group_id=37116&atid=418822
		////////////////////////////////////////////////////////////////////////////
		//
		// 4068 (unknown pragma)
		//   only used in scumm engine to mark code sections
		//
		// 4100 (unreferenced formal parameter)
		//
		// 4103 (alignment changed after including header, may be due to missing #pragma pack(pop))
		//   used by pack-start / pack-end
		//
		// 4127 (conditional expression is constant)
		//   used in a lot of engines
		//
		// 4244 ('conversion' conversion from 'type1' to 'type2', possible loss of data)
		//   throws tons and tons of warnings, most of them false positives
		//
		// 4250 ('class1' : inherits 'class2::member' via dominance)
		//   two or more members have the same name. Should be harmless
		//
		// 4310 (cast truncates constant value)
		//   used in some engines
		//
		// 4351 (new behavior: elements of array 'array' will be default initialized)
		//   a change in behavior in Visual Studio 2005. We want the new behavior, so it can be disabled
		//
		// 4512 ('class' : assignment operator could not be generated)
		//   some classes use const items and the default assignment operator cannot be generated
		//
		// 4702 (unreachable code)
		//   mostly thrown after error() calls (marked as NORETURN)
		//
		// 4706 (assignment within conditional expression)
		//   used in a lot of engines
		//
		// 4800 ('type' : forcing value to bool 'true' or 'false' (performance warning))
		//
		// 4996 ('function': was declared deprecated)
		//   disabling it removes all the non-standard unsafe functions warnings (strcpy_s, etc.)
		//
		// 6211 (Leaking memory <pointer> due to an exception. Consider using a local catch block to clean up memory)
		//   we disable exceptions
		//
		// 6204 (possible buffer overrun in call to <function>: use of unchecked parameter <variable>)
		// 6385 (invalid data: accessing <buffer name>, the readable size is <size1> bytes, but <size2> bytes may be read)
		// 6386 (buffer overrun: accessing <buffer name>, the writable size is <size1> bytes, but <size2> bytes may be written)
		//   give way too many false positives
		//
		////////////////////////////////////////////////////////////////////////////
		//
		// 4189 (local variable is initialized but not referenced)
		//   false positive in lure engine
		//
		// 4355 ('this' : used in base member initializer list)
		//   only disabled for specific engines where it is used in a safe way
		//
		// 4510 ('class' : default constructor could not be generated)
		//
		// 4511 ('class' : copy constructor could not be generated)
		//
		// 4610 (object 'class' can never be instantiated - user-defined constructor required)
		//   "correct" but harmless (as is 4510)
		//
		////////////////////////////////////////////////////////////////////////////

		SET_GLOBAL_WARNINGS("4068", "4100", "4103", "4127", "4244", "4250", "4310", "4351", "4512", "4702", "4706", "4800", "4996", "6204", "6211", "6385", "6386");
		SET_WARNINGS("agi", "4510", "4610");
		SET_WARNINGS("agos", "4511");
		SET_WARNINGS("lure", "4189", "4355");
		SET_WARNINGS("kyra", "4355");
		SET_WARNINGS("m4", "4355");

		if (msvcVersion == 8 || msvcVersion == 9)
			provider = new CreateProjectTool::VisualStudioProvider(globalWarnings, projectWarnings, msvcVersion);
		else
			provider = new CreateProjectTool::MSBuildProvider(globalWarnings, projectWarnings, msvcVersion);

		break;
	}

	provider->createProject(setup);

	delete provider;
}

namespace {
std::string unifyPath(const std::string &path) {
	std::string result = path;
	std::replace(result.begin(), result.end(), '\\', '/');
	return result;
}

std::string getLastPathComponent(const std::string &path) {
	std::string::size_type pos = path.find_last_of('/');
	if (pos == std::string::npos)
		return path;
	else
		return path.substr(pos + 1);
}

void displayHelp(const char *exe) {
	using std::cout;

	cout << "Usage:\n"
	     << exe << " path\\to\\source [optional options]\n"
	     << "\n"
	     << " Creates project files for the ScummVM source located at \"path\\to\\source\".\n"
	        " The project files will be created in the directory where tool is run from and\n"
	        " will include \"path\\to\\source\" for relative file paths, thus be sure that you\n"
	        " pass a relative file path like \"..\\..\\trunk\".\n"
	        "\n"
	        " Additionally there are the following switches for changing various settings:\n"
	        "\n"
	        "Project specific settings:\n"
	        " --codeblock              build Code::Blocks project files\n"
	        " --msvc                   build Visual Studio project files\n"
	        " --file-prefix prefix     allow overwriting of relative file prefix in the\n"
	        "                          MSVC project files. By default the prefix is the\n"
	        "                          \"path\\to\\source\" argument\n"
	        " --output-dir path        overwrite path, where the project files are placed\n"
	        "                          By default this is \".\", i.e. the current working\n"
	        "                          directory\n"
	        "\n"
	        "MSVC specific settings:\n"
	        " --msvc-version version   set the targeted MSVC version. Possible values:\n"
	        "                           8 stands for \"Visual Studio 2005\"\n"
	        "                           9 stands for \"Visual Studio 2008\"\n"
	        "                           10 stands for \"Visual Studio 2010\"\n"
	        "                           The default is \"9\", thus \"Visual Studio 2008\"\n"
	        " --build-events           Run custom build events as part of the build\n"
	        "                          (default: false)\n"
	        "\n"
	        "ScummVM engine settings:\n"
	        " --list-engines           list all available engines and their default state\n"
	        " --enable-engine          enable building of the engine with the name \"engine\"\n"
	        " --disable-engine         disable building of the engine with the name \"engine\"\n"
	        " --enable-all-engines     enable building of all engines\n"
	        " --disable-all-engines    disable building of all engines\n"
	        "\n"
	        "ScummVM optional feature settings:\n"
	        " --enable-name            enable inclusion of the feature \"name\"\n"
	        " --disable-name           disable inclusion of the feature \"name\"\n"
	        "\n"
	        " There are the following features available:\n"
	        "\n";

	cout << "   state  |       name      |     description\n\n";
	const FeatureList features = getAllFeatures();
	cout.setf(std::ios_base::left, std::ios_base::adjustfield);
	for (FeatureList::const_iterator i = features.begin(); i != features.end(); ++i)
		cout << ' ' << (i->enable ? " enabled" : "disabled") << " | " << std::setw((std::streamsize)15) << i->name << std::setw((std::streamsize)0) << " | " << i->description << '\n';
	cout.setf(std::ios_base::right, std::ios_base::adjustfield);
}

typedef StringList TokenList;

/**
 * Takes a given input line and creates a list of tokens out of it.
 *
 * A token in this context is separated by whitespaces. A special case
 * are quotation marks though. A string inside quotation marks is treated
 * as single token, even when it contains whitespaces.
 *
 * Thus for example the input:
 * foo bar "1 2 3 4" ScummVM
 * will create a list with the following entries:
 * "foo", "bar", "1 2 3 4", "ScummVM"
 * As you can see the quotation marks will get *removed* too.
 *
 * @param input The text to be tokenized.
 * @return A list of tokens.
 */
TokenList tokenize(const std::string &input);

/**
 * Try to parse a given line and create an engine definition
 * out of the result.
 *
 * This may take *any* input line, when the line is not used
 * to define an engine the result of the function will be "false".
 *
 * Note that the contents of "engine" are undefined, when this
 * function returns "false".
 *
 * @param line Text input line.
 * @param engine Reference to an object, where the engine information
 *               is to be stored in.
 * @return "true", when parsing succeeded, "false" otherwise.
 */
bool parseEngine(const std::string &line, EngineDesc &engine);
} // End of anonymous namespace

EngineDescList parseConfigure(const std::string &srcDir) {
	std::string configureFile = srcDir + "/configure";

	std::ifstream configure(configureFile.c_str());
	if (!configure)
		return EngineDescList();

	std::string line;
	EngineDescList engines;

	for (;;) {
		std::getline(configure, line);
		if (configure.eof())
			break;

		if (configure.fail())
			error("Failed while reading from " + configureFile);

		EngineDesc desc;
		if (parseEngine(line, desc))
			engines.push_back(desc);
	}

	return engines;
}

bool isSubEngine(const std::string &name, const EngineDescList &engines) {
	for (EngineDescList::const_iterator i = engines.begin(); i != engines.end(); ++i) {
		if (std::find(i->subEngines.begin(), i->subEngines.end(), name) != i->subEngines.end())
			return true;
	}

	return false;
}

bool setEngineBuildState(const std::string &name, EngineDescList &engines, bool enable) {
	if (enable && isSubEngine(name, engines)) {
		// When we enable a sub engine, we need to assure that the parent is also enabled,
		// thus we enable both sub engine and parent over here.
		EngineDescList::iterator engine = std::find(engines.begin(), engines.end(), name);
		if (engine != engines.end()) {
			engine->enable = enable;

			for (engine = engines.begin(); engine != engines.end(); ++engine) {
				if (std::find(engine->subEngines.begin(), engine->subEngines.end(), name) != engine->subEngines.end()) {
					engine->enable = true;
					break;
				}
			}

			return true;
		}
	} else {
		EngineDescList::iterator engine = std::find(engines.begin(), engines.end(), name);
		if (engine != engines.end()) {
			engine->enable = enable;

			// When we disable an einge, we also need to disable all the sub engines.
			if (!enable && !engine->subEngines.empty()) {
				for (StringList::const_iterator j = engine->subEngines.begin(); j != engine->subEngines.end(); ++j) {
					EngineDescList::iterator subEngine = std::find(engines.begin(), engines.end(), *j);
					if (subEngine != engines.end())
						subEngine->enable = false;
				}
			}

			return true;
		}
	}

	return false;
}

StringList getEngineDefines(const EngineDescList &engines) {
	StringList result;

	for (EngineDescList::const_iterator i = engines.begin(); i != engines.end(); ++i) {
		if (i->enable) {
			std::string define = "ENABLE_" + i->name;
			std::transform(define.begin(), define.end(), define.begin(), toupper);
			result.push_back(define);
		}
	}

	return result;
}

namespace {
bool parseEngine(const std::string &line, EngineDesc &engine) {
	// Format:
	// add_engine engine_name "Readable Description" enable_default ["SubEngineList"]
	TokenList tokens = tokenize(line);

	if (tokens.size() < 4)
		return false;

	TokenList::const_iterator token = tokens.begin();

	if (*token != "add_engine")
		return false;
	++token;

	engine.name = *token; ++token;
	engine.desc = *token; ++token;
	engine.enable = (*token == "yes"); ++token;
	if (token != tokens.end())
		engine.subEngines = tokenize(*token);

	return true;
}

TokenList tokenize(const std::string &input) {
	TokenList result;

	std::string::size_type sIdx = input.find_first_not_of(" \t");
	std::string::size_type nIdx = std::string::npos;

	if (sIdx == std::string::npos)
		return result;

	do {
		if (input.at(sIdx) == '\"') {
			++sIdx;
			nIdx = input.find_first_of('\"', sIdx);
		} else {
			nIdx = input.find_first_of(' ', sIdx);
		}

		if (nIdx != std::string::npos) {
			result.push_back(input.substr(sIdx, nIdx - sIdx));
			sIdx = input.find_first_not_of(" \t", nIdx + 1);
		} else {
			result.push_back(input.substr(sIdx));
			break;
		}
	} while (sIdx != std::string::npos);

	return result;
}
} // End of anonymous namespace

namespace {
const Feature s_features[] = {
	// Libraries
	{    "libz",        "USE_ZLIB", "zlib", true, "zlib (compression) support" },
	{     "mad",         "USE_MAD", "libmad", true, "libmad (MP3) support" },
	{  "vorbis",      "USE_VORBIS", "libvorbisfile_static libvorbis_static libogg_static", true, "Ogg Vorbis support" },
	{    "flac",        "USE_FLAC", "libFLAC_static", true, "FLAC support" },
	{   "png",           "USE_PNG", "libpng", true, "libpng support" },
	{   "theora",  "USE_THEORADEC", "libtheora_static", true, "Theora decoding support" },
	{   "mpeg2",       "USE_MPEG2", "libmpeg2", false, "mpeg2 codec for cutscenes" },

	// ScummVM feature flags
	{     "scalers",     "USE_SCALERS", "", true, "Scalers" },
	{   "hqscalers",  "USE_HQ_SCALERS", "", true, "HQ scalers" },
	{       "16bit",   "USE_RGB_COLOR", "", true, "16bit color support" },
	{     "mt32emu",     "USE_MT32EMU", "", true, "integrated MT-32 emulator" },
	{        "nasm",        "USE_NASM", "", true, "IA-32 assembly support" }, // This feature is special in the regard, that it needs additional handling.
	{      "opengl",      "USE_OPENGL", "opengl32", true, "OpenGL support" },
	{      "indeo3",      "USE_INDEO3", "", true, "Indeo3 codec support"},
	{ "translation", "USE_TRANSLATION", "", true, "Translation support" },
	{  "langdetect",  "USE_DETECTLANG", "", true, "System language detection support" } // This feature actually depends on "translation", there
	                                                                                    // is just no current way of properly detecting this...
};
} // End of anonymous namespace

FeatureList getAllFeatures() {
	const size_t featureCount = sizeof(s_features) / sizeof(s_features[0]);

	FeatureList features;
	for (size_t i = 0; i < featureCount; ++i)
		features.push_back(s_features[i]);

	return features;
}

StringList getFeatureDefines(const FeatureList &features) {
	StringList defines;

	for (FeatureList::const_iterator i = features.begin(); i != features.end(); ++i) {
		if (i->enable && i->define && i->define[0])
			defines.push_back(i->define);
	}

	return defines;
}

StringList getFeatureLibraries(const FeatureList &features) {
	StringList libraries;

	for (FeatureList::const_iterator i = features.begin(); i != features.end(); ++i) {
		if (i->enable && i->libraries && i->libraries[0]) {
			StringList fLibraries = tokenize(i->libraries);
			libraries.splice(libraries.end(), fLibraries);
		}
	}

	return libraries;
}

bool setFeatureBuildState(const std::string &name, FeatureList &features, bool enable) {
	FeatureList::iterator i = std::find(features.begin(), features.end(), name);
	if (i != features.end()) {
		i->enable = enable;
		return true;
	} else {
		return false;
	}
}

namespace CreateProjectTool {

//////////////////////////////////////////////////////////////////////////
// Utilities
//////////////////////////////////////////////////////////////////////////

std::string convertPathToWin(const std::string &path) {
	std::string result = path;
	std::replace(result.begin(), result.end(), '/', '\\');
	return result;
}

std::string getIndent(const int indentation) {
	std::string result;
	for (int i = 0; i < indentation; ++i)
		result += '\t';
	return result;
}

void splitFilename(const std::string &fileName, std::string &name, std::string &ext) {
	const std::string::size_type dot = fileName.find_last_of('.');
	name = (dot == std::string::npos) ? fileName : fileName.substr(0, dot);
	ext = (dot == std::string::npos) ? std::string() : fileName.substr(dot + 1);
}

bool producesObjectFile(const std::string &fileName) {
	std::string n, ext;
	splitFilename(fileName, n, ext);

	if (ext == "cpp" || ext == "c" || ext == "asm")
		return true;
	else
		return false;
}

/**
 * Checks whether the give file in the specified directory is present in the given
 * file list.
 *
 * This function does as special match against the file list. It will not take file
 * extensions into consideration, when the extension of a file in the specified
 * directory is one of "h", "cpp", "c" or "asm".
 *
 * @param dir Parent directory of the file.
 * @param fileName File name to match.
 * @param fileList List of files to match against.
 * @return "true" when the file is in the list, "false" otherwise.
 */
bool isInList(const std::string &dir, const std::string &fileName, const StringList &fileList) {
	std::string compareName, extensionName;
	splitFilename(fileName, compareName, extensionName);

	if (!extensionName.empty())
		compareName += '.';

	for (StringList::const_iterator i = fileList.begin(); i != fileList.end(); ++i) {
		if (i->compare(0, dir.size(), dir))
			continue;

		// When no comparison name is given, we try to match whether a subset of
		// the given directory should be included. To do that we must assure that
		// the first character after the substring, having the same size as dir, must
		// be a path delimiter.
		if (compareName.empty()) {
			if (i->size() >= dir.size() + 1 && i->at(dir.size()) == '/')
				return true;
			else
				continue;
		}

		const std::string lastPathComponent = getLastPathComponent(*i);
		if (!producesObjectFile(fileName) && extensionName != "h") {
			if (fileName == lastPathComponent)
				return true;
		} else {
			if (!lastPathComponent.compare(0, compareName.size(), compareName))
				return true;
		}
	}

	return false;
}

/**
 * A strict weak compare predicate for sorting a list of
 * "FileNode *" entries.
 *
 * It will sort directory nodes before file nodes.
 *
 * @param l Left-hand operand.
 * @param r Right-hand operand.
 * @return "true" if and only if l should be sorted before r.
 */
bool compareNodes(const FileNode *l, const FileNode *r) {
	if (!l) {
		return false;
	} else if (!r) {
		return true;
	} else {
		if (l->children.empty() && !r->children.empty()) {
			return false;
		} else if (!l->children.empty() && r->children.empty()) {
			return true;
		} else {
			return l->name < r->name;
		}
	}
}

/**
 * Returns a list of all files and directories in the specified
 * path.
 *
 * @param dir Directory which should be listed.
 * @return List of all children.
 */
FileList listDirectory(const std::string &dir) {
	FileList result;
#if defined(_WIN32) || defined(WIN32)
	WIN32_FIND_DATA fileInformation;
	HANDLE fileHandle = FindFirstFile((dir + "/*").c_str(), &fileInformation);

	if (fileHandle == INVALID_HANDLE_VALUE)
		return result;

	do {
		if (fileInformation.cFileName[0] == '.')
			continue;

		result.push_back(FSNode(fileInformation.cFileName, (fileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0));
	} while (FindNextFile(fileHandle, &fileInformation) == TRUE);

	FindClose(fileHandle);
#else
	DIR *dirp = opendir(dir.c_str());
	struct dirent *dp = NULL;

	if (dirp == NULL)
		return result;

	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_name[0] == '.')
			continue;

		struct stat st;
		if (stat((dir + '/' + dp->d_name).c_str(), &st))
			continue;

		result.push_back(FSNode(dp->d_name, S_ISDIR(st.st_mode)));
	}

	closedir(dirp);
#endif
	return result;
}

/**
 * Scans the specified directory against files, which should be included
 * in the project files. It will not include files present in the exclude list.
 *
 * @param dir Directory in which to search for files.
 * @param includeList Files to include in the project.
 * @param excludeList Files to exclude from the project.
 * @return Returns a file node for the specific directory.
 */
FileNode *scanFiles(const std::string &dir, const StringList &includeList, const StringList &excludeList) {
	FileList files = listDirectory(dir);

	if (files.empty())
		return 0;

	FileNode *result = new FileNode(dir);
	assert(result);

	for (FileList::const_iterator i = files.begin(); i != files.end(); ++i) {
		if (i->isDirectory) {
			const std::string subDirName = dir + '/' + i->name;
			if (!isInList(subDirName, std::string(), includeList))
				continue;

			FileNode *subDir = scanFiles(subDirName, includeList, excludeList);

			if (subDir) {
				subDir->name = i->name;
				result->children.push_back(subDir);
			}
			continue;
		}

		if (isInList(dir, i->name, excludeList))
			continue;

		std::string name, ext;
		splitFilename(i->name, name, ext);

		if (ext != "h") {
			if (!isInList(dir, i->name, includeList))
				continue;
		}

		FileNode *child = new FileNode(i->name);
		assert(child);
		result->children.push_back(child);
	}

	if (result->children.empty()) {
		delete result;
		return 0;
	} else {
		result->children.sort(compareNodes);
		return result;
	}
}

//////////////////////////////////////////////////////////////////////////
// Project Provider methods
//////////////////////////////////////////////////////////////////////////
ProjectProvider::ProjectProvider(StringList &global_warnings, std::map<std::string, StringList> &project_warnings, const int version)
	: _version(version), _globalWarnings(global_warnings), _projectWarnings(project_warnings) {
}

void ProjectProvider::createProject(const BuildSetup &setup) {
	_uuidMap = createUUIDMap(setup);

	// We also need to add the UUID of the main project file.
	const std::string svmUUID = _uuidMap["scummvm"] = createUUID();

	// Create Solution/Workspace file
	createWorkspace(setup);

	StringList in, ex;

	// Create engine project files
	for (UUIDMap::const_iterator i = _uuidMap.begin(); i != _uuidMap.end(); ++i) {
		if (i->first == "scummvm")
			continue;

		in.clear(); ex.clear();
		const std::string moduleDir = setup.srcDir + "/engines/" + i->first;

		createModuleList(moduleDir, setup.defines, in, ex);
		createProjectFile(i->first, i->second, setup, moduleDir, in, ex);
	}

	// Last but not least create the main ScummVM project file.
	in.clear(); ex.clear();

	// File list for the ScummVM project file
	createModuleList(setup.srcDir + "/backends", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/backends/platform/sdl", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/base", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/common", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/engines", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/graphics", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/gui", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/sound", setup.defines, in, ex);
	createModuleList(setup.srcDir + "/sound/softsynth/mt32", setup.defines, in, ex);

	// Resource files
	in.push_back(setup.srcDir + "/icons/scummvm.ico");
	in.push_back(setup.srcDir + "/dists/scummvm.rc");

	// Various text files
	in.push_back(setup.srcDir + "/AUTHORS");
	in.push_back(setup.srcDir + "/COPYING");
	in.push_back(setup.srcDir + "/COPYING.LGPL");
	in.push_back(setup.srcDir + "/COPYRIGHT");
	in.push_back(setup.srcDir + "/NEWS");
	in.push_back(setup.srcDir + "/README");
	in.push_back(setup.srcDir + "/TODO");

	// Create the scummvm project file.
	createProjectFile("scummvm", svmUUID, setup, setup.srcDir, in, ex);

	// Create other misc. build files
	createOtherBuildFiles(setup);
}

ProjectProvider::UUIDMap ProjectProvider::createUUIDMap(const BuildSetup &setup) const {
	UUIDMap result;

	for (EngineDescList::const_iterator i = setup.engines.begin(); i != setup.engines.end(); ++i) {
		if (!i->enable || isSubEngine(i->name, setup.engines))
			continue;

		result[i->name] = createUUID();
	}

	return result;
}

std::string ProjectProvider::createUUID() const {
#if defined(_WIN32) || defined(WIN32)
	UUID uuid;
	if (UuidCreate(&uuid) != RPC_S_OK)
		error("UuidCreate failed");

	unsigned char *string = 0;
	if (UuidToStringA(&uuid, &string) != RPC_S_OK)
		error("UuidToStringA failed");

	std::string result = std::string((char *)string);
	std::transform(result.begin(), result.end(), result.begin(), toupper);
	RpcStringFreeA(&string);
	return result;
#else
	unsigned char uuid[16];

	for (int i = 0; i < 16; ++i)
		uuid[i] = (unsigned char)((std::rand() / (double)(RAND_MAX)) * 0xFF);

	uuid[8] &= 0xBF; uuid[8] |= 0x80;
	uuid[6] &= 0x4F; uuid[6] |= 0x40;

	std::stringstream uuidString;
	uuidString << std::hex << std::uppercase << std::setfill('0');
	for (int i = 0; i < 16; ++i) {
		uuidString << std::setw(2) << (int)uuid[i];
		if (i == 3 || i == 5 || i == 7 || i == 9) {
			uuidString << std::setw(0) << '-';
		}
	}

	return uuidString.str();
#endif
}

void ProjectProvider::addFilesToProject(const std::string &dir, std::ofstream &projectFile,
                                        const StringList &includeList, const StringList &excludeList,
                                        const std::string &filePrefix) {
	// Check for duplicate object file names
	StringList duplicate;

	for (StringList::const_iterator i = includeList.begin(); i != includeList.end(); ++i) {
		const std::string fileName = getLastPathComponent(*i);

		// Leave out non object file names.
		if (fileName.size() < 2 || fileName.compare(fileName.size() - 2, 2, ".o"))
			continue;

		// Check whether an duplicate has been found yet
		if (std::find(duplicate.begin(), duplicate.end(), fileName) != duplicate.end())
			continue;

		// Search for duplicates
		StringList::const_iterator j = i; ++j;
		for (; j != includeList.end(); ++j) {
			if (fileName == getLastPathComponent(*j)) {
				duplicate.push_back(fileName);
				break;
			}
		}
	}

	FileNode *files = scanFiles(dir, includeList, excludeList);

	writeFileListToProject(*files, projectFile, 0, duplicate, std::string(), filePrefix + '/');

	delete files;
}

void ProjectProvider::createModuleList(const std::string &moduleDir, const StringList &defines, StringList &includeList, StringList &excludeList) const {
	const std::string moduleMkFile = moduleDir + "/module.mk";
	std::ifstream moduleMk(moduleMkFile.c_str());
	if (!moduleMk)
		error(moduleMkFile + " is not present");

	includeList.push_back(moduleMkFile);

	std::stack<bool> shouldInclude;
	shouldInclude.push(true);

	bool hadModule = false;
	std::string line;
	for (;;) {
		std::getline(moduleMk, line);

		if (moduleMk.eof())
			break;

		if (moduleMk.fail())
			error("Failed while reading from " + moduleMkFile);

		TokenList tokens = tokenize(line);
		if (tokens.empty())
			continue;

		TokenList::const_iterator i = tokens.begin();
		if (*i == "MODULE") {
			if (hadModule)
				error("More than one MODULE definition in " + moduleMkFile);
			// Format: "MODULE := path/to/module"
			if (tokens.size() < 3)
				error("Malformed MODULE definition in " + moduleMkFile);
			++i;
			if (*i != ":=")
				error("Malformed MODULE definition in " + moduleMkFile);
			++i;

			std::string moduleRoot = unifyPath(*i);
			if (moduleDir.compare(moduleDir.size() - moduleRoot.size(), moduleRoot.size(), moduleRoot))
				error("MODULE root " + moduleRoot + " does not match base dir " + moduleDir);

			hadModule = true;
		} else if (*i == "MODULE_OBJS") {
			if (tokens.size() < 3)
				error("Malformed MODULE_OBJS definition in " + moduleMkFile);
			++i;

			// This is not exactly correct, for example an ":=" would usually overwrite
			// all already added files, but since we do only save the files inside
			// includeList or excludeList currently, we couldn't handle such a case easily.
			// (includeList and excludeList should always preserve their entries, not added
			// by this function, thus we can't just clear them on ":=" or "=").
			// But hopefully our module.mk files will never do such things anyway.
			if (*i != ":=" && *i != "+=" && *i != "=")
				error("Malformed MODULE_OBJS definition in " + moduleMkFile);

			++i;

			while (i != tokens.end()) {
				if (*i == "\\") {
					std::getline(moduleMk, line);
					tokens = tokenize(line);
					i = tokens.begin();
				} else {
					if (shouldInclude.top())
						includeList.push_back(moduleDir + "/" + unifyPath(*i));
					else
						excludeList.push_back(moduleDir + "/" + unifyPath(*i));
					++i;
				}
			}
		} else if (*i == "ifdef") {
			if (tokens.size() < 2)
				error("Malformed ifdef in " + moduleMkFile);
			++i;

			if (std::find(defines.begin(), defines.end(), *i) == defines.end())
				shouldInclude.push(false);
			else
				shouldInclude.push(true);
		} else if (*i == "ifndef") {
			if (tokens.size() < 2)
				error("Malformed ifndef in " + moduleMkFile);
			++i;

			if (std::find(defines.begin(), defines.end(), *i) == defines.end())
				shouldInclude.push(true);
			else
				shouldInclude.push(false);
		} else if (*i == "else") {
			shouldInclude.top() = !shouldInclude.top();
		} else if (*i == "endif") {
			if (shouldInclude.size() <= 1)
				error("endif without ifdef found in " + moduleMkFile);
			shouldInclude.pop();
		} else if (*i == "elif") {
			error("Unsupported operation 'elif' in " + moduleMkFile);
		} else if (*i == "ifeq") {
			//XXX
			shouldInclude.push(false);
		}
	}

	if (shouldInclude.size() != 1)
		error("Malformed file " + moduleMkFile);
}

} // End of anonymous namespace

void error(const std::string &message) {
	std::cerr << "ERROR: " << message << "!" << std::endl;
	std::exit(-1);
}
