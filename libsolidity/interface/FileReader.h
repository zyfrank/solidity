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
#pragma once

#include <libsolidity/interface/ImportRemapper.h>
#include <libsolidity/interface/ReadFile.h>

#include <boost/filesystem.hpp>

#include <map>
#include <set>

namespace solidity::frontend
{

/// FileReader - used for progressively loading source code.
///
/// It is used in solc to load files from CLI parameters, stdin, or from JSON and
/// also used in the solc language server where solc is a long running process.
class FileReader
{
public:
	using StringMap = std::map<SourceUnitName, SourceCode>;
	using PathMap = std::map<SourceUnitName, boost::filesystem::path>;
	using FileSystemPathSet = std::set<boost::filesystem::path>;

	/// Constructs a FileReader with a base path and a set of allowed directories that
	/// will be used when requesting files from this file reader instance.
	explicit FileReader(
		boost::filesystem::path _basePath = {},
		FileSystemPathSet _allowedDirectories = {}
	):
		m_allowedDirectories(std::move(_allowedDirectories)),
		m_sourceCodes()
	{
		setBasePath(_basePath);
	}

	void setBasePath(boost::filesystem::path const& _path);
	boost::filesystem::path const& basePath() const noexcept { return m_basePath; }

	void allowDirectory(boost::filesystem::path _path) { m_allowedDirectories.insert(std::move(_path)); }
	FileSystemPathSet const& allowedDirectories() const noexcept { return m_allowedDirectories; }

	StringMap const& sourceCodes() const noexcept { return m_sourceCodes; }

	/// Retrieves the source code for a given source unit name.
	SourceCode const& sourceCode(SourceUnitName const& _sourceUnitName) const { return m_sourceCodes.at(_sourceUnitName); }

	/// Resets all sources to the given map of source unit name to source codes.
	/// Does not enforce @a allowedDirectories().
	void setSources(StringMap _sources);

	/// Adds the source code under a source unit name created by normalizing the file path.
	/// Does not enforce @a allowedDirectories().
	void setSource(boost::filesystem::path const& _path, SourceCode _source);

	/// Receives a @p _sourceUnitName that refers to a source unit in compiler's virtual filesystem
	/// and attempts to interpret it as a path and read the corresponding file from disk.
	/// The read will only succeed if the canonical path of the file is within one of the @a allowedDirectories().
	/// @param _kind must be equal to "source". Other values are not supported.
	/// @return Content of the loaded file or an error message. If the operation succeeds, a copy of
	/// the content is retained in @a sourceCodes() under the key of @a _sourceUnitName. If the key
	/// already exists, previous content is discarded.
	frontend::ReadCallback::Result readFile(std::string const& _kind, std::string const& _sourceUnitName);

	frontend::ReadCallback::Callback reader()
	{
		return [this](std::string const& _kind, std::string const& _path) { return readFile(_kind, _path); };
	}

	/// Normalizes a filesystem path in a way that removes small, inconsequential differences. Specifically:
	/// - Makes the path absolute. If it is empty, it becomes the current working directory.
	/// - Collapses redundant . and .. segments.
	/// - Squashes sequences of multiple slashes into one.
	/// - If the filesystem is case-insensitive, ensures that the actual case from disk is used.
	/// - Removes named root if it's the same as in the current working directory.
	/// - Does NOT resolve symlinks.
	/// - Preserves a single slash at the end of the path (if present).
	/// The specified path must actually exist.
	static boost::filesystem::path normalizeCLIPathForVFS(boost::filesystem::path const& _path);

	/// Returns true if all the path components of @a _prefix are present at the beginning of @a _path.
	/// Both paths must be absolute and normalized (no . or .. segments, no multiple consecutive slashes).
	/// Paths are treated as case-sensitive. Does not require the path to actually exist in the
	/// filesystem and does not follow symlinks. Only considers whole segments, e.g. /abc/d is not
	/// considered a prefix of /abc/def. Both paths must be non-empty.
	static bool isPathPrefix(boost::filesystem::path _prefix, boost::filesystem::path const& _path);

	/// Returns a copy of @a _path with @a _prefix removed from the beginning.
	/// Only works for arguments for which isPathPrefix() returns true.
	/// Returns '.' if @a _path and @_prefix are identical.
	static boost::filesystem::path stripPathPrefix(boost::filesystem::path _prefix, boost::filesystem::path const& _path);

private:
	/// Base path, used for resolving relative paths in imports.
	boost::filesystem::path m_basePath;

	/// list of allowed directories to read files from
	FileSystemPathSet m_allowedDirectories;

	/// map of input files to source code strings
	StringMap m_sourceCodes;
};

}
