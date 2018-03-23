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
/** @file Compiler.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#pragma once

#include <libdevcore/Common.h>

#include <libsolidity/interface/EVMVersion.h>

#include <string>
#include <vector>

namespace dev
{
namespace eth
{

using ReadCallback = std::function<std::string(std::string const&)>;

std::string parseLLL(std::string const& _src);
std::string compileLLLToAsm(std::string const& _src, solidity::EVMVersion _evmVersion, bool _opt = true, std::vector<std::string>* _errors = nullptr, ReadCallback const& _readFile = ReadCallback());
bytes compileLLL(std::string const& _src, solidity::EVMVersion _evmVersion, bool _opt = true, std::vector<std::string>* _errors = nullptr, ReadCallback const& _readFile = ReadCallback());

}
}
