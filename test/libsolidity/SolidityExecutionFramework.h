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
/**
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Framework for executing Solidity contracts and testing them against C++ implementation.
 */

#pragma once

#include <functional>

#include <test/ExecutionFramework.h>

#include <libsolidity/interface/CompilerStack.h>
#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/interface/SourceReferenceFormatter.h>

namespace dev
{
namespace solidity
{

namespace test
{

class SolidityExecutionFramework: public dev::test::ExecutionFramework
{

public:
	SolidityExecutionFramework();

	virtual bytes const& compileAndRunWithoutCheck(
		std::string const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		std::vector<bytes> const& _arguments = std::vector<bytes>(),
		std::map<std::string, dev::test::Address> const& _libraryAddresses = std::map<std::string, dev::test::Address>()
	) override
	{
		bytes const& bytecode = compileContract(_sourceCode, _contractName, _libraryAddresses);
		sendMessage(_arguments, "", bytecode, true, _value);
		if (m_status.empty()) {
			return m_status;
		}
		return bytecode;
	}

	bytes compileContract(
		std::string const& _sourceCode,
		std::string const& _contractName = "",
		std::map<std::string, dev::test::Address> const& _libraryAddresses = std::map<std::string, dev::test::Address>()
	)
	{
		// Silence compiler version warning
		std::string sourceCode = "pragma solidity >=0.0;\n" + _sourceCode;
		m_compiler.reset(false);
		m_compiler.addSource("", sourceCode);
		m_compiler.setLibraries(_libraryAddresses);
		m_compiler.setOptimiserSettings(m_optimize, m_optimizeRuns);
		if (!m_compiler.compile())
		{
			auto scannerFromSourceName = [&](std::string const& _sourceName) -> solidity::Scanner const& { return m_compiler.scanner(_sourceName); };
			SourceReferenceFormatter formatter(std::cerr, scannerFromSourceName);

			for (auto const& error: m_compiler.errors())
				formatter.printExceptionInformation(
					*error,
					(error->type() == Error::Type::Warning) ? "Warning" : "Error"
				);
			BOOST_ERROR("Compiling contract failed");
		}
		eth::LinkerObject obj = m_compiler.object(_contractName.empty() ? m_compiler.lastContractName() : _contractName);
		BOOST_REQUIRE(obj.linkReferences.empty());
		return obj.bytecode;
	}

protected:
	dev::solidity::CompilerStack m_compiler;
};

}
}
} // end namespaces

