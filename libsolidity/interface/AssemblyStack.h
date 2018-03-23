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
 * Full assembly stack that can support EVM-assembly and JULIA as input and EVM, EVM1.5 and
 * eWasm as output.
 */

#pragma once

#include <libsolidity/interface/ErrorReporter.h>
#include <libsolidity/interface/EVMVersion.h>

#include <libevmasm/LinkerObject.h>

#include <string>
#include <memory>

namespace dev
{
namespace solidity
{
class Scanner;
namespace assembly
{
struct AsmAnalysisInfo;
struct Block;
}

struct MachineAssemblyObject
{
	std::shared_ptr<eth::LinkerObject> bytecode;
	std::string assembly;
};

/*
 * Full assembly stack that can support EVM-assembly and JULIA as input and EVM, EVM1.5 and
 * eWasm as output.
 */
class AssemblyStack
{
public:
	enum class Language { JULIA, Assembly, StrictAssembly };
	enum class Machine { EVM, EVM15, eWasm };

	explicit AssemblyStack(EVMVersion _evmVersion = EVMVersion(), Language _language = Language::Assembly):
		m_language(_language), m_evmVersion(_evmVersion), m_errorReporter(m_errors)
	{}

	/// @returns the scanner used during parsing
	Scanner const& scanner() const;

	/// Runs parsing and analysis steps, returns false if input cannot be assembled.
	/// Multiple calls overwrite the previous state.
	bool parseAndAnalyze(std::string const& _sourceName, std::string const& _source);

	/// Runs analysis step on the supplied block, returns false if input cannot be assembled.
	/// Multiple calls overwrite the previous state.
	bool analyze(assembly::Block const& _block, Scanner const* _scanner = nullptr);

	/// Run the assembly step (should only be called after parseAndAnalyze).
	MachineAssemblyObject assemble(Machine _machine) const;

	/// @returns the errors generated during parsing, analysis (and potentially assembly).
	ErrorList const& errors() const { return m_errors; }

	/// Pretty-print the input after having parsed it.
	std::string print() const;

private:
	bool analyzeParsed();

	Language m_language = Language::Assembly;
	EVMVersion m_evmVersion;

	std::shared_ptr<Scanner> m_scanner;

	bool m_analysisSuccessful = false;
	std::shared_ptr<assembly::Block> m_parserResult;
	std::shared_ptr<assembly::AsmAnalysisInfo> m_analysisInfo;
	ErrorList m_errors;
	ErrorReporter m_errorReporter;
};

}
}
