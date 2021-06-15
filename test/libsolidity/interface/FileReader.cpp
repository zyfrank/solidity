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

/// Unit tests for libsolidity/interface/FileReader.h

#include <libsolidity/interface/FileReader.h>

#include <test/Common.h>
#include <test/TemporaryDirectory.h>
#include <test/libsolidity/util/SoltestErrors.h>

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace solidity::test;


namespace solidity::frontend::test
{

BOOST_AUTO_TEST_SUITE(FileReaderTest)

BOOST_AUTO_TEST_CASE(normalizeCLIPathForVFS_absolute_path)
{
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/") == "/");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a") == "/a");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/") == "/a/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/.") == "/a/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/b") == "/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/b/") == "/a/b/");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/./b/") == "/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/../a/b/") == "/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/b/c/..") == "/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/b/c/../") == "/a/b/");
}

BOOST_AUTO_TEST_CASE(normalizeCLIPathForVFS_relative_path)
{
	TemporaryDirectory tempDir("file-reader-test-");
	boost::filesystem::path workDir = tempDir.path() / "x/y/z";
	create_directories(tempDir.path() / "x/y/z");
	TemporaryWorkingDirectory tempWorkDir(tempDir.path() / "x/y/z");
	soltestAssert(tempDir.path().is_absolute(), "");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("") == tempDir.path() / "x/y/z");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS(".") == tempDir.path() / "x/y/z/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("./") == tempDir.path() / "x/y/z/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../") == tempDir.path() / "x/y/");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a") == tempDir.path() / "x/y/z/a");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/") == tempDir.path() / "x/y/z/a/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/.") == tempDir.path() / "x/y/z/a/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/b") == tempDir.path() / "x/y/z/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/b/") == tempDir.path() / "x/y/z/a/b/");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../a/b") == tempDir.path() / "x/y/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../../a/b") == tempDir.path() / "x/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("./a/b") == tempDir.path() / "x/y/z/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("././a/b") == tempDir.path() / "x/y/z/a/b");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/./b/") == tempDir.path() / "x/y/z/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/../a/b/") == tempDir.path() / "x/y/z/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/b/c/..") == tempDir.path() / "x/y/z/a/b");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("a/b/c/../") == tempDir.path() / "x/y/z/a/b/");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../../a/.././../p/../q/../a/b") == tempDir.path() / "a/b");

}

BOOST_AUTO_TEST_CASE(normalizeCLIPathForVFS_redundant_slashes)
{
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("///") == "/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("////") == "/");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("////a/b/") == "/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a//b/") == "/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a////b/") == "/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/b//") == "/a/b/");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/a/b////") == "/a/b/");
}

BOOST_AUTO_TEST_CASE(normalizeCLIPathForVFS_double_slash_root)
{
	TemporaryDirectory tempDir("file-reader-test-");
	TemporaryWorkingDirectory tempWorkDir(tempDir.path());

	// Leading // has special meaning so it gets preserved.
	// FIXME: // should not turn into current working directory
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("//") == tempDir.path());
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("//a/b/") == "//a/b/");

}

BOOST_AUTO_TEST_CASE(normalizeCLIPathForVFS_path_beyond_root)
{
	TemporaryWorkingDirectory tempWorkDir("/");

	// FIXME: These should have the .. segments collapsed
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/..") == "/..");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/../") == "/../");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/../..") == "/../..");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/../a") == "/../a");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("/../a/../..") == "/../..");

	BOOST_TEST(FileReader::normalizeCLIPathForVFS("..") == "/..");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../") == "/../");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../..") == "/../..");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../a") == "/../a");
	BOOST_TEST(FileReader::normalizeCLIPathForVFS("../a/../..") == "/../..");
}

BOOST_AUTO_TEST_CASE(isPathPrefix_file_prefix)
{
	BOOST_TEST(FileReader::isPathPrefix("/", "/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/contract.sol", "/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/contract.sol/", "/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/contract.sol/.", "/contract.sol"));

	BOOST_TEST(FileReader::isPathPrefix("/", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a/", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a/bc", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a/bc/def/contract.sol", "/a/bc/def/contract.sol"));

	BOOST_TEST(FileReader::isPathPrefix("/", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a/", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a/bc", "/a/bc/def/contract.sol"));
	BOOST_TEST(FileReader::isPathPrefix("/a/bc/def/contract.sol", "/a/bc/def/contract.sol"));

	BOOST_TEST(!FileReader::isPathPrefix("/contract.sol", "/token.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/contract", "/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/contract.sol", "/contract"));
	BOOST_TEST(!FileReader::isPathPrefix("/contract.so", "/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/contract.sol", "/contract.so"));

	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/contract.sol", "/a/b/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/contract.sol", "/a/b/c/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/contract.sol", "/a/b/c/d/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/d/contract.sol", "/a/b/c/contract.sol"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/contract.sol", "/contract.sol"));
}

BOOST_AUTO_TEST_CASE(isPathPrefix_directory_prefix)
{
	BOOST_TEST(FileReader::isPathPrefix("/", "/"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/", "/"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c", "/"));

	BOOST_TEST(FileReader::isPathPrefix("/", "/a/bc/"));
	BOOST_TEST(FileReader::isPathPrefix("/a", "/a/bc/"));
	BOOST_TEST(FileReader::isPathPrefix("/a/", "/a/bc/"));
	BOOST_TEST(FileReader::isPathPrefix("/a/bc", "/a/bc/"));
	BOOST_TEST(FileReader::isPathPrefix("/a/bc/", "/a/bc/"));

	BOOST_TEST(!FileReader::isPathPrefix("/a", "/b/"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/", "/b/"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/contract.sol", "/a/b/"));

	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/", "/a/b/"));
	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c", "/a/b/"));

	BOOST_TEST(!FileReader::isPathPrefix("/a/b/c/", "//a/b/c/"));
	BOOST_TEST(!FileReader::isPathPrefix("//a/b/c/", "/a/b/c/"));
}

BOOST_AUTO_TEST_CASE(stripPathPrefix_file_prefix)
{
	BOOST_TEST(FileReader::stripPathPrefix("/", "/contract.sol") == "contract.sol");
	BOOST_TEST(FileReader::stripPathPrefix("/contract.sol", "/contract.sol") == ".");

	BOOST_TEST(FileReader::stripPathPrefix("/", "/a/bc/def/contract.sol") == "a/bc/def/contract.sol");
	BOOST_TEST(FileReader::stripPathPrefix("/a", "/a/bc/def/contract.sol") == "bc/def/contract.sol");
	BOOST_TEST(FileReader::stripPathPrefix("/a/", "/a/bc/def/contract.sol") == "bc/def/contract.sol");
	BOOST_TEST(FileReader::stripPathPrefix("/a/bc", "/a/bc/def/contract.sol") == "def/contract.sol");
	BOOST_TEST(FileReader::stripPathPrefix("/a/bc/def/", "/a/bc/def/contract.sol") == "contract.sol");
	BOOST_TEST(FileReader::stripPathPrefix("/a/bc/def/contract.sol", "/a/bc/def/contract.sol") == ".");
}

BOOST_AUTO_TEST_CASE(stripPathPrefix_directory_prefix)
{
	BOOST_TEST(FileReader::stripPathPrefix("/", "/") == ".");

	BOOST_TEST(FileReader::stripPathPrefix("/", "/a/bc/def/") == "a/bc/def/");
	BOOST_TEST(FileReader::stripPathPrefix("/a", "/a/bc/def/") == "bc/def/");
	BOOST_TEST(FileReader::stripPathPrefix("/a/", "/a/bc/def/") == "bc/def/");
	BOOST_TEST(FileReader::stripPathPrefix("/a/bc", "/a/bc/def/") == "def/");
	BOOST_TEST(FileReader::stripPathPrefix("/a/bc/def/", "/a/bc/def/") == ".");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace solidity::frontend::test
