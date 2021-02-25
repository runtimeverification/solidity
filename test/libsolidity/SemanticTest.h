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

#pragma once

#include <test/libsolidity/util/TestFileParser.h>
#include <test/libsolidity/util/TestFunctionCall.h>
#include <test/libsolidity/SolidityExecutionFramework.h>
#include <test/libsolidity/AnalysisFramework.h>
#include <test/TestCase.h>
#include <liblangutil/Exceptions.h>
#include <libsolutil/AnsiColorized.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <utility>

namespace solidity::frontend::test
{

/**
 * Class that represents a semantic test (or end-to-end test) and allows running it as part of the
 * boost unit test environment or isoltest. It reads the Solidity source and an additional comment
 * section from the given file. This comment section should define a set of functions to be called
 * and an expected result they return after being executed.
 */
class SemanticTest: public SolidityExecutionFramework, public EVMVersionRestrictedTestCase
{
public:
	static std::unique_ptr<TestCase> create(Config const& _options)
	{ return std::make_unique<SemanticTest>(_options.filename, _options.evmVersion, _options.vmPaths, _options.enforceCompileViaYul); }

	explicit SemanticTest(std::string const& _filename, langutil::EVMVersion _evmVersion, std::vector<boost::filesystem::path> const& _vmPaths, bool _enforceViaYul = false);

	TestResult run(std::ostream& _stream, std::string const& _linePrefix = "", bool _formatted = false) override;
	void printSource(std::ostream &_stream, std::string const& _linePrefix = "", bool _formatted = false) const override;
	void printUpdatedExpectations(std::ostream& _stream, std::string const& _linePrefix = "") const override;
	void printUpdatedSettings(std::ostream& _stream, std::string const& _linePrefix = "") override;

	/// Instantiates a test file parser that parses the additional comment section at the end of
	/// the input stream \param _stream. Each function call is represented using a `FunctionCallTest`
	/// and added to the list of call to be executed when `run()` is called.
	/// Throws if parsing expectations failed.
	void parseExpectations(std::istream& _stream);

	/// Compiles and deploys currently held source.
	/// Returns true if deployment was successful, false otherwise.
	bool deploy(std::string const& _contractName, u256 const& _value, std::vector<bytes> const& _arguments, std::map<std::string, solidity::test::Address> const& _libraries = {});
private:
	TestResult runTest(std::ostream& _stream, std::string const& _linePrefix, bool _formatted, bool _compileViaYul, bool _compileToEwasm);
	SourceMap m_sources;
	std::size_t m_lineOffset;
	std::vector<TestFunctionCall> m_tests;
	bool m_runWithYul = false;
	bool m_runWithEwasm = false;
	bool m_runWithoutYul = true;
	bool m_enforceViaYul = false;
	bool m_runWithABIEncoderV1Only = false;
	bool m_allowNonExistingFunctions = false;
	bool m_compileViaYulCanBeSet = false;
};

}
