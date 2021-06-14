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

#include <string_view>
#include <tuple>

namespace solidity::langutil
{

/// Parses doxygen-style key/value pairs from a string.
///
/// - Each line may contain a key/value pair.
/// - Keys must start with '@' (but are not exposed with their leading '@').
/// - Key key name currently may contain any character except a leading '@' and a trailing space (0x20).
/// - Values are space-trimmed on both sides are located on the right side of the key.
/// - Currently values CANNOT spam multiple lines, only single lines.
///
/// Example:
///
/// @code for (auto [key, value, ok]: KeyValuePairsParser::parse("@foo bar")) fmt::print("{} {} {}\n", key, value, ok);
///
struct KeyValuePairsParser
{
	static KeyValuePairsParser parse(std::string_view _text)
	{
		return KeyValuePairsParser{_text};
	}

	struct iterator
	{
		constexpr iterator(std::string_view _text) noexcept: m_text{_text} { ++*this; }
		constexpr bool operator==(iterator const& _other) const noexcept { return m_text == _other.m_text; }
		constexpr bool operator!=(iterator const& _other) const noexcept { return !(*this == _other); }

		constexpr auto operator*() const noexcept { return std::tuple{m_key, m_value, m_ok}; }

		constexpr iterator& invalidate() noexcept
		{
				m_ok = false;
				m_text = {};
				m_key = {};
				m_value = {};
				return *this;
		}

		// Consumes ONE key/value pair.
		constexpr iterator& operator++() noexcept
		{
			auto const isWhitespace = [](char c) constexpr { return c == ' ' || c == '\t'; };
			auto const isNewLine = [](char c) constexpr { return c == '\r' || c == '\n'; };

			m_value = {};
			m_key = {};

			if (!m_ok || m_text.empty())
				return *this;

			// skip leading whitespace
			while (!m_text.empty() && (isWhitespace(m_text.front()) || isNewLine(m_text.front())))
				m_text.remove_prefix(1);

			// consume @keyname
			if (m_text.empty() || m_text.front() != '@')
				return invalidate();
			m_text.remove_prefix(1);

			size_t i = 0;
			while (i < m_text.size() && !isWhitespace(m_text.at(i)))
				++i;

			m_key = m_text.substr(0, i);
			m_text.remove_prefix(i);

			// disallow empty keys
			if (m_key.empty())
				return invalidate();

			// skip whitespace
			while (!m_text.empty() && isWhitespace(m_text.front()))
				m_text.remove_prefix(1);

			// consume value
			i = 0;
			while (i < m_text.size() && !isNewLine(m_text.at(i)))
				++i;
			m_value = m_text.substr(0, i);
			m_text.remove_prefix(i);

			// trim value
			while (!m_value.empty() && isWhitespace(m_value.back()))
				m_value.remove_suffix(1);

			// consume possible newline
			i = 0;
			while (i < m_text.size() && isNewLine(m_text.at(i)))
				++i;
			m_text.remove_prefix(i);

			return *this;
		}

		std::string_view m_text;

		std::string_view m_key;
		std::string_view m_value;
		bool m_ok = true;
	};

	iterator begin() const noexcept { return iterator(m_text); }
	iterator end() const noexcept { return iterator({}); }

	std::string_view m_text;
};

} // end namespace
