/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0

/// Unit tests for solc/CommandLineInterface.h

#include <solc/CommandLineInterface.h>

#include <test/Common.h>
#include <test/TemporaryDirectory.h>
#include <test/libsolidity/util/SoltestErrors.h>

#include <boost/test/unit_test.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace std;
using namespace solidity::frontend;
using namespace solidity::test;

using PathSet = set<boost::filesystem::path>;

namespace
{

struct OptionsReaderAndMessages
{
	bool success;
	CommandLineOptions options;
	FileReader reader;
	optional<string> standardJsonInput;
	string stdoutContent;
	string stderrContent;
};

OptionsReaderAndMessages parseCommandLineAndReadInputFiles(vector<string> const& commandLine)
{
	size_t argc = commandLine.size();
	vector<char const*> argv(commandLine.size() + 1);

	// argv[argc] typically contains NULL
	argv[argc] = nullptr;

	for (size_t i = 0; i < argc; ++i)
		argv[i] = commandLine[i].c_str();

	stringstream sin, sout, serr;
	CommandLineInterface cli(sin, sout, serr);
	bool success = cli.parseArguments(static_cast<int>(argc), argv.data());
	success = success && cli.readInputFiles();

	return {success, cli.options(), cli.fileReader(), cli.standardJsonInput(), sout.str(), serr.str()};
}

CommandLineOptions defaultCommandLineOptions()
{
	CommandLineOptions options;

	options.expectedExecutionsPerDeployment = 200;
	options.initializeModelChecker = true;
	options.modelCheckerSettings = {
		ModelCheckerContracts::Default(),
		ModelCheckerEngine::None(),
		ModelCheckerTargets::Default(),
		nullopt,
	};

	return options;
}

void createEmptyFilesWithParentDirs(set<boost::filesystem::path> const& _paths)
{
	for (boost::filesystem::path const& path: _paths)
	{
		if (!path.parent_path().empty())
			boost::filesystem::create_directories(path.parent_path());

		ofstream file;
		file.exceptions(ofstream::badbit | ofstream::failbit);
		file.open(path);
		soltestAssert(boost::filesystem::exists(path), "");
	}
}

} // namespace

namespace solidity::frontend::test
{

BOOST_AUTO_TEST_SUITE(CommandLineInterfaceTest)

BOOST_AUTO_TEST_CASE(cli_input)
{
	TemporaryDirectory tempDir1("file-reader-test-");
	TemporaryDirectory tempDir2("file-reader-test-");
	createEmptyFilesWithParentDirs({tempDir1.path() / "input1.sol"});
	createEmptyFilesWithParentDirs({tempDir2.path() / "input2.sol"});

	vector<ImportRemapper::Remapping> expectedRemappings = {
		{"", "a", "b/c/d"},
		{"a", "b", "c/d/e/"},
	};
	map<string, string> expectedSources = {
		{"<stdin>", "\n"},
		{(tempDir1.path() / "input1.sol").string(), ""},
		{(tempDir2.path() / "input2.sol").string(), ""},
	};
	PathSet expectedAllowedPaths = {tempDir1.path(), tempDir2.path(), "b/c", "c/d/e"};

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({
		"solc",
		"a=b/c/d",
		(tempDir1.path() / "input1.sol").string(),
		(tempDir2.path() / "input2.sol").string(),
		"a:b=c/d/e/",
		"-",
	});

	BOOST_TEST(result.success);
	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST((result.options.inputMode == InputMode::Compiler));
	BOOST_TEST(result.options.addStdin);
	BOOST_TEST(result.options.remappings == expectedRemappings);
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedPaths);
}

BOOST_AUTO_TEST_CASE(cli_ignore_missing_some_files_exist)
{
	TemporaryDirectory tempDir1("file-reader-test-");
	TemporaryDirectory tempDir2("file-reader-test-");
	createEmptyFilesWithParentDirs({tempDir1.path() / "input1.sol"});

	// NOTE: Allowed paths should not be added for skipped files.
	map<string, string> expectedSources = {{(tempDir1.path() / "input1.sol").string(), ""}};
	PathSet expectedAllowedPaths = {tempDir1.path()};

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({
		"solc",
		(tempDir1.path() / "input1.sol").string(),
		(tempDir2.path() / "input2.sol").string(),
		"--ignore-missing",
	});
	BOOST_TEST(result.success);
	BOOST_TEST(result.stderrContent == "\"" + (tempDir2.path() / "input2.sol").string() + "\" is not found. Skipping.\n");
	BOOST_TEST((result.options.inputMode == InputMode::Compiler));
	BOOST_TEST(!result.options.addStdin);
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedPaths);
}

BOOST_AUTO_TEST_CASE(cli_ignore_missing_no_files_exist)
{
	TemporaryDirectory tempDir("file-reader-test-");

	string expectedMessage =
		"\"" + (tempDir.path() / "input1.sol").string() + "\" is not found. Skipping.\n"
		"\"" + (tempDir.path() / "input2.sol").string() + "\" is not found. Skipping.\n"
		"All specified input files either do not exist or are not regular files.\n";

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({
		"solc",
		(tempDir.path() / "input1.sol").string(),
		(tempDir.path() / "input2.sol").string(),
		"--ignore-missing",
	});
	BOOST_TEST(!result.success);
	BOOST_TEST(result.stderrContent == expectedMessage);
}

BOOST_AUTO_TEST_CASE(cli_not_a_file)
{
	TemporaryDirectory tempDir("file-reader-test-");

	string expectedMessage = "\"" + tempDir.path().string() + "\" is not a valid file.\n";

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({"solc", tempDir.path().string()});
	BOOST_TEST(!result.success);
	BOOST_TEST(result.stderrContent == expectedMessage);
}

BOOST_AUTO_TEST_CASE(standard_json_base_path)
{
	TemporaryDirectory tempDir("file-reader-test-");

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({
		"solc",
		"--standard-json",
		"--base-path=" + tempDir.path().string(),
	});
	BOOST_TEST(result.success);
	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST((result.options.inputMode == InputMode::StandardJson));
	BOOST_TEST(result.options.addStdin);
	BOOST_TEST(result.options.sourceFilePaths.empty());
	BOOST_TEST(result.reader.sourceCodes().empty());
	BOOST_TEST(result.reader.allowedDirectories().empty());
	BOOST_TEST(result.reader.basePath() == tempDir.path());
}

BOOST_AUTO_TEST_CASE(standard_json_no_input_file)
{
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({"solc", "--standard-json"});
	BOOST_TEST(result.success);
	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST((result.options.inputMode == InputMode::StandardJson));
	BOOST_TEST(result.options.addStdin);
	BOOST_TEST(result.options.sourceFilePaths.empty());
	BOOST_TEST(result.reader.sourceCodes().empty());
	BOOST_TEST(result.reader.allowedDirectories().empty());
}

BOOST_AUTO_TEST_CASE(standard_json_dash)
{
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({"solc", "--standard-json", "-"});
	BOOST_TEST(result.success);
	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST((result.options.inputMode == InputMode::StandardJson));
	BOOST_TEST(result.options.addStdin);
	BOOST_TEST(result.reader.sourceCodes().empty());
	BOOST_TEST(result.reader.allowedDirectories().empty());
}

BOOST_AUTO_TEST_CASE(standard_json_one_input_file)
{
	TemporaryDirectory tempDir("file-reader-test-");
	createEmptyFilesWithParentDirs({tempDir.path() / "input.json"});

	vector<string> commandLine = {"solc", "--standard-json", (tempDir.path() / "input.json").string()};
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);
	BOOST_TEST(result.success);
	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST((result.options.inputMode == InputMode::StandardJson));
	BOOST_TEST(!result.options.addStdin);
	BOOST_TEST(result.options.sourceFilePaths == PathSet{tempDir.path() / "input.json"});
	BOOST_TEST(result.reader.allowedDirectories().empty());
}

BOOST_AUTO_TEST_CASE(standard_json_two_input_files)
{
	string expectedMessage =
		"Too many input files for --standard-json.\n"
		"Please either specify a single file name or provide its content on standard input.\n";

	vector<string> commandLine = {"solc", "--standard-json", "input1.json", "input2.json"};
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);
	BOOST_TEST(!result.success);
	BOOST_TEST(result.stderrContent == expectedMessage);
}

BOOST_AUTO_TEST_CASE(standard_json_one_input_file_and_stdin)
{
	string expectedMessage =
		"Too many input files for --standard-json.\n"
		"Please either specify a single file name or provide its content on standard input.\n";

	vector<string> commandLine = {"solc", "--standard-json", "input1.json", "-"};
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);
	BOOST_TEST(!result.success);
	BOOST_TEST(result.stderrContent == expectedMessage);
}

BOOST_AUTO_TEST_CASE(standard_json_ignore_missing)
{
	TemporaryDirectory tempDir("file-reader-test-");

	// This option is pretty much useless Standard JSON mode.
	string expectedMessage =
		"\"" + (tempDir.path() / "input.json").string() + "\" is not found. Skipping.\n"
		"All specified input files either do not exist or are not regular files.\n";

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles({
		"solc",
		"--standard-json",
		(tempDir.path() / "input.json").string(),
		"--ignore-missing",
	});
	BOOST_TEST(!result.success);
	BOOST_TEST(result.stderrContent == expectedMessage);
}

BOOST_AUTO_TEST_CASE(standard_json_remapping)
{
	string expectedMessage =
		"Import remappings are not accepted on the command line in Standard JSON mode.\n"
		"Please put them under 'settings.remappings' in the JSON input.\n";

	vector<string> commandLine = {"solc", "--standard-json", "a=b"};
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);
	BOOST_TEST(!result.success);
	BOOST_TEST(result.stderrContent == expectedMessage);
}

BOOST_AUTO_TEST_CASE(cli_paths_to_source_unit_names_no_base_path)
{
	TemporaryDirectory tempDirCurrent("file-reader-test-");
	TemporaryDirectory tempDirOther("file-reader-test-");
	TemporaryWorkingDirectory tempWorkDir(tempDirCurrent.path());
	soltestAssert(tempDirCurrent.path().is_absolute(), "");
	soltestAssert(tempDirOther.path().is_absolute(), "");

	vector<string> commandLine = {
		"solc",
		"contract1.sol",                                    // Relative path
		"c/d/contract2.sol",                                // Relative path with subdirectories
		tempDirCurrent.path().string() + "/contract3.sol",  // Absolute path inside working dir
		tempDirOther.path().string() + "/contract4.sol",    // Absolute path outside of working dir
	};

	CommandLineOptions expectedOptions = defaultCommandLineOptions();
	expectedOptions.sourceFilePaths = {
		"contract1.sol",
		"c/d/contract2.sol",
		tempDirCurrent.path() / "contract3.sol",
		tempDirOther.path() / "contract4.sol",
	};

	map<string, string> expectedSources = {
		{"contract1.sol", ""},
		{"c/d/contract2.sol", ""},
		{"contract3.sol", ""},
		{tempDirOther.path().string() + "/contract4.sol", ""},
	};

	FileReader::FileSystemPathSet expectedAllowedDirectories = {
		tempDirCurrent.path() / "c/d",
		tempDirCurrent.path(),
		tempDirOther.path(),
	};

	createEmptyFilesWithParentDirs(expectedOptions.sourceFilePaths);
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);

	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST(result.stdoutContent == "");
	BOOST_REQUIRE(result.success);
	BOOST_TEST((result.options == expectedOptions));
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedDirectories);
	BOOST_TEST(result.reader.basePath() == expectedOptions.basePath);
}

BOOST_AUTO_TEST_CASE(cli_paths_to_source_unit_names_base_path_same_as_work_dir)
{
	TemporaryDirectory tempDirCurrent("file-reader-test-");
	TemporaryDirectory tempDirOther("file-reader-test-");
	TemporaryWorkingDirectory tempWorkDir(tempDirCurrent.path());
	soltestAssert(tempDirCurrent.path().is_absolute(), "");
	soltestAssert(tempDirOther.path().is_absolute(), "");

	vector<string> commandLine = {
		"solc",
		"--base-path=" + tempDirCurrent.path().string(),
		"contract1.sol",                                    // Relative path
		"c/d/contract2.sol",                                // Relative path with subdirectories
		tempDirCurrent.path().string() + "/contract3.sol",  // Absolute path inside working dir
		tempDirOther.path().string() + "/contract4.sol",    // Absolute path outside of working dir
	};

	CommandLineOptions expectedOptions = defaultCommandLineOptions();
	expectedOptions.sourceFilePaths = {
		"contract1.sol",
		"c/d/contract2.sol",
		tempDirCurrent.path() / "contract3.sol",
		tempDirOther.path() / "contract4.sol",
	};
	expectedOptions.basePath = tempDirCurrent.path();

	map<string, string> expectedSources = {
		{"contract1.sol", ""},
		{"c/d/contract2.sol", ""},
		{"contract3.sol", ""},
		{tempDirOther.path().string() + "/contract4.sol", ""},
	};

	FileReader::FileSystemPathSet expectedAllowedDirectories = {
		tempDirCurrent.path() / "c/d",
		tempDirCurrent.path(),
		tempDirOther.path(),
	};

	createEmptyFilesWithParentDirs(expectedOptions.sourceFilePaths);
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);

	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST(result.stdoutContent == "");
	BOOST_REQUIRE(result.success);
	BOOST_TEST((result.options == expectedOptions));
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedDirectories);
	BOOST_TEST(result.reader.basePath() == expectedOptions.basePath);
}

BOOST_AUTO_TEST_CASE(cli_paths_to_source_unit_names_base_path_different_from_work_dir)
{
	TemporaryDirectory tempDirCurrent("file-reader-test-");
	TemporaryDirectory tempDirOther("file-reader-test-");
	TemporaryDirectory tempDirBase("file-reader-test-");
	TemporaryWorkingDirectory tempWorkDir(tempDirCurrent.path());
	soltestAssert(tempDirCurrent.path().is_absolute(), "");
	soltestAssert(tempDirOther.path().is_absolute(), "");
	soltestAssert(tempDirBase.path().is_absolute(), "");

	vector<string> commandLine = {
		"solc",
		"--base-path=" + tempDirBase.path().string(),
		"contract1.sol",                                    // Relative path
		"c/d/contract2.sol",                                // Relative path with subdirectories
		tempDirCurrent.path().string() + "/contract3.sol",  // Absolute path inside working dir
		tempDirOther.path().string() + "/contract4.sol",    // Absolute path outside of working dir
		tempDirBase.path().string() + "/contract5.sol",     // Absolute path inside base path
	};

	CommandLineOptions expectedOptions = defaultCommandLineOptions();
	expectedOptions.sourceFilePaths = {
		"contract1.sol",
		"c/d/contract2.sol",
		tempDirCurrent.path() / "contract3.sol",
		tempDirOther.path() / "contract4.sol",
		tempDirBase.path() / "contract5.sol",
	};
	expectedOptions.basePath = tempDirBase.path();

	map<string, string> expectedSources = {
		{tempDirCurrent.path().string() + "/contract1.sol", ""},
		{tempDirCurrent.path().string() + "/c/d/contract2.sol", ""},
		{tempDirCurrent.path().string() + "/contract3.sol", ""},
		{tempDirOther.path().string() + "/contract4.sol", ""},
		{"contract5.sol", ""},
	};

	FileReader::FileSystemPathSet expectedAllowedDirectories = {
		tempDirCurrent.path() / "c/d",
		tempDirCurrent.path(),
		tempDirOther.path(),
		tempDirBase.path(),
	};

	createEmptyFilesWithParentDirs(expectedOptions.sourceFilePaths);
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);

	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST(result.stdoutContent == "");
	BOOST_REQUIRE(result.success);
	BOOST_TEST((result.options == expectedOptions));
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedDirectories);
	BOOST_TEST(result.reader.basePath() == expectedOptions.basePath);
}

BOOST_AUTO_TEST_CASE(cli_paths_to_source_unit_names_relative_base_path)
{
	TemporaryDirectory tempDirCurrent("file-reader-test-");
	TemporaryDirectory tempDirOther("file-reader-test-");
	TemporaryWorkingDirectory tempWorkDir(tempDirCurrent.path());
	soltestAssert(tempDirCurrent.path().is_absolute(), "");
	soltestAssert(tempDirOther.path().is_absolute(), "");

	vector<string> commandLine = {
		"solc",
		"--base-path=base",
		"contract1.sol",                                        // Relative path outside of base path
		"base/contract2.sol",                                   // Relative path inside base path
		tempDirCurrent.path().string() + "/contract3.sol",      // Absolute path inside working dir
		tempDirCurrent.path().string() + "/base/contract4.sol", // Absolute path inside base path
		tempDirOther.path().string() + "/contract5.sol",        // Absolute path outside of working dir
		tempDirOther.path().string() + "/base/contract6.sol",   // Absolute path outside of working dir
	};

	CommandLineOptions expectedOptions = defaultCommandLineOptions();
	expectedOptions.sourceFilePaths = {
		"contract1.sol",
		"base/contract2.sol",
		tempDirCurrent.path() / "contract3.sol",
		tempDirCurrent.path() / "base/contract4.sol",
		tempDirOther.path() / "contract5.sol",
		tempDirOther.path() / "base/contract6.sol",
	};
	expectedOptions.basePath = "base";

	map<string, string> expectedSources = {
		{tempDirCurrent.path().string() + "/contract1.sol", ""},
		{"contract2.sol", ""},
		{tempDirCurrent.path().string() + "/contract3.sol", ""},
		{"contract4.sol", ""},
		{tempDirOther.path().string() + "/contract5.sol", ""},
		{tempDirOther.path().string() + "/base/contract6.sol", ""},
	};

	FileReader::FileSystemPathSet expectedAllowedDirectories = {
		tempDirCurrent.path() / "base",
		tempDirCurrent.path(),
		tempDirOther.path(),
		tempDirOther.path() / "base",
	};

	createEmptyFilesWithParentDirs(expectedOptions.sourceFilePaths);
	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);

	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST(result.stdoutContent == "");
	BOOST_REQUIRE(result.success);
	BOOST_TEST((result.options == expectedOptions));
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedDirectories);
	BOOST_TEST(result.reader.basePath() == tempDirCurrent.path() / "base");
}

BOOST_AUTO_TEST_CASE(cli_paths_to_source_unit_names_normalization_and_weird_names)
{
	TemporaryDirectory tempDir("file-reader-test-");
	create_directories(tempDir.path() / "x/y/z");
	TemporaryWorkingDirectory tempWorkDir(tempDir.path() / "x/y/z");
	soltestAssert(tempDir.path().is_absolute(), "");

	string doubleSlashRootDir = "/" + tempDir.path().generic_string();
	soltestAssert(doubleSlashRootDir[0] == '/' && doubleSlashRootDir[1] == '/', "");
	soltestAssert(doubleSlashRootDir[2] != '/', "");

	vector<string> commandLine = {
		"solc",

		// URLs. We interpret them as local paths.
		"file://c/d/contract1.sol",
		"file:///c/d/contract2.sol",
		"https://example.com/contract3.sol",

		// Redundant slashes
		"a/b//contract4.sol",
		"a/b///contract5.sol",
		"a/b////contract6.sol",

		// Dot segments
		"./a/b/contract7.sol",
		"././a/b/contract8.sol",
		"a/./b/contract9.sol",
		"a/././b/contract10.sol",

		// Dot dot segments
		"../a/b/contract11.sol",
		"../../a/b/contract12.sol",
		"a/../b/contract13.sol",
		"a/b/../../contract14.sol",
		tempDir.path().string() + "/x/y/z/a/../b/contract15.sol",
		tempDir.path().string() + "/x/y/z/a/b/../../contract16.sol",

		// Dot dot segments going beyond filesystem root
		"/.." + tempDir.path().generic_string() + "/contract17.sol",
		"/../.." + tempDir.path().generic_string() + "/contract18.sol",

		// Path with two slashes (often treated specially)
		doubleSlashRootDir + "/contract19.sol",

		// Name conflict with source unit name of stdin
		"<stdin>",

#if !defined(_WIN32)
		// Windows paths on non-Windows systems.
		// Note that on Windows we tested them already just by using absolute paths.
		"a\\b\\contract20.sol",
		"C:\\a\\b\\contract21.sol",
#endif
	};

	CommandLineOptions expectedOptions = defaultCommandLineOptions();
	expectedOptions.sourceFilePaths = {
		"file://c/d/contract1.sol",
		"file:///c/d/contract2.sol",
		"https://example.com/contract3.sol",

		"a/b//contract4.sol",
		"a/b///contract5.sol",
		"a/b////contract6.sol",

		"./a/b/contract7.sol",
		"././a/b/contract8.sol",
		"a/./b/contract9.sol",
		"a/././b/contract10.sol",

		"../a/b/contract11.sol",
		"../../a/b/contract12.sol",
		"a/../b/contract13.sol",
		"a/b/../../contract14.sol",
		tempDir.path().string() + "/x/y/z/a/../b/contract15.sol",
		tempDir.path().string() + "/x/y/z/a/b/../../contract16.sol",

		"/.." + tempDir.path().string() + "/contract17.sol",
		"/../.." + tempDir.path().string() + "/contract18.sol",

		doubleSlashRootDir + "/contract19.sol",
		"<stdin>",

#if !defined(_WIN32)
		"a\\b\\contract20.sol",
		"C:\\a\\b\\contract21.sol",
#endif
	};

	map<string, string> expectedSources = {
		{"file:/c/d/contract1.sol", ""},
		{"file:/c/d/contract2.sol", ""},
		{"https:/example.com/contract3.sol", ""},

		{"a/b/contract4.sol", ""},
		{"a/b/contract5.sol", ""},
		{"a/b/contract6.sol", ""},

		{"a/b/contract7.sol", ""},
		{"a/b/contract8.sol", ""},
		{"a/b/contract9.sol", ""},
		{"a/b/contract10.sol", ""},

		{tempDir.path().string() + "/x/y/a/b/contract11.sol", ""},
		{tempDir.path().string() + "/x/a/b/contract12.sol", ""},
		{"b/contract13.sol", ""},
		{"contract14.sol", ""},
		{"b/contract15.sol", ""},
		{"contract16.sol", ""},

		{"/.." + tempDir.path().string() + "/contract17.sol", ""},
		{"/../.." + tempDir.path().string() + "/contract18.sol", ""},

		{doubleSlashRootDir + "/contract19.sol", ""},
		{"<stdin>", ""},

#if !defined(_WIN32)
		{"a\\b\\contract20.sol", ""},
		{"C:\\a\\b\\contract21.sol", ""},
#endif
	};

	FileReader::FileSystemPathSet expectedAllowedDirectories = {
		tempDir.path() / "x/y/z/file:/c/d",
		tempDir.path() / "x/y/z/https:/example.com",
		tempDir.path() / "x/y/z/a/b",
		tempDir.path() / "x/y/z",
		tempDir.path() / "x/y/z/b",
		tempDir.path() / "x/y/a/b",
		tempDir.path() / "x/a/b",
		doubleSlashRootDir,
		tempDir.path(),
	};

	createEmptyFilesWithParentDirs(expectedOptions.sourceFilePaths);

	OptionsReaderAndMessages result = parseCommandLineAndReadInputFiles(commandLine);

	BOOST_TEST(result.stderrContent == "");
	BOOST_TEST(result.stdoutContent == "");
	BOOST_REQUIRE(result.success);
	BOOST_TEST((result.options == expectedOptions));
	BOOST_TEST(result.reader.sourceCodes() == expectedSources);
	BOOST_TEST(result.reader.allowedDirectories() == expectedAllowedDirectories);
	BOOST_TEST(result.reader.basePath() == expectedOptions.basePath);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace solidity::frontend::test
