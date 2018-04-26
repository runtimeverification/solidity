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
 * @date 2015
 * Unit tests for the gas estimator.
 */

#include <test/libsolidity/SolidityExecutionFramework.h>
#include <libevmasm/GasMeter.h>
#include <libevmasm/KnownState.h>
#include <libevmasm/PathGasMeter.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/interface/GasEstimator.h>
#include <libsolidity/interface/SourceReferenceFormatter.h>

using namespace std;
using namespace dev::eth;
using namespace dev::solidity;
using namespace dev::test;

namespace dev
{
namespace solidity
{
namespace test
{

class GasMeterTestFramework: public SolidityExecutionFramework
{
public:
	GasMeterTestFramework() { }
	void compile(string const& _sourceCode)
	{
		m_compiler.reset(false);
		m_compiler.addSource("", "pragma solidity >=0.0;\n" + _sourceCode);
		m_compiler.setOptimiserSettings(dev::test::Options::get().optimize);
		m_compiler.setEVMVersion(m_evmVersion);
		BOOST_REQUIRE_MESSAGE(m_compiler.compile(), "Compiling contract failed");

		AssemblyItems const* items = nullptr; //m_compiler.runtimeAssemblyItems(m_compiler.lastContractName());
		ASTNode const& sourceUnit = m_compiler.ast("");
		BOOST_REQUIRE(items != nullptr);
		m_gasCosts = GasEstimator::breakToStatementLevel(
			GasEstimator(dev::test::Options::get().evmVersion()).structuralEstimation(*items, vector<ASTNode const*>({&sourceUnit})),
			{&sourceUnit}
		);
	}

	void testCreationTimeGas(string const& _sourceCode)
	{
/*
		compileAndRun(_sourceCode);
		auto state = make_shared<KnownState>();
		PathGasMeter meter(*m_compiler.assemblyItems(m_compiler.lastContractName()), dev::test::Options::get().evmVersion());
		GasMeter::GasConsumption gas = meter.estimateMax(0, state);
		u256 bytecodeSize(m_compiler.runtimeObject(m_compiler.lastContractName()).bytecode.size());
		// costs for deployment
		gas += bytecodeSize * GasCosts::createDataGas;
		// costs for transaction
		gas += gasForTransaction(m_compiler.object(m_compiler.lastContractName()).bytecode, vector<bytes>());

		BOOST_REQUIRE(!gas.isInfinite);
		BOOST_CHECK_EQUAL(gas.value, m_gasUsed);
*/
		BOOST_REQUIRE(false);
	}

	/// Compares the gas computed by PathGasMeter for the given signature (but unknown arguments)
	/// against the actual gas usage computed by the VM on the given set of argument variants.
	void testRunTimeGas(string const& _sig, vector<vector<bytes>> _argumentVariants)
	{
/*
		u256 gasUsed = 0;
		GasMeter::GasConsumption gas;
		for (vector<bytes> const& arguments: _argumentVariants)
		{
			callContractFunctionNoEncoding(_sig, arguments);
			gasUsed = max(gasUsed, m_gasUsed);
			gas = max(gas, gasForTransaction(_sig, arguments));
		}

		gas += GasEstimator(dev::test::Options::get().evmVersion()).functionalEstimation(
			*m_compiler.runtimeAssemblyItems(m_compiler.lastContractName()),
			_sig
		);
		BOOST_REQUIRE(!gas.isInfinite);
		BOOST_CHECK_EQUAL(gas.value, m_gasUsed);
*/
		BOOST_REQUIRE(false);
	}

	static GasMeter::GasConsumption gasForTransaction(bytes const& _data, vector<bytes> const& _arguments)
	{
		GasMeter::GasConsumption gas = GasCosts::txCreateGas;
		bytes txData = rlpEncode(_data, _arguments);	
		for (auto i: txData)
			gas += i != 0 ? GasCosts::txDataNonZeroGas : GasCosts::txDataZeroGas;
		return gas;
	}

	static GasMeter::GasConsumption gasForTransaction(string const& _sig, vector<bytes> const& _arguments)
	{
		GasMeter::GasConsumption gas = GasCosts::txGas;
		bytes txData = rlpEncode(asBytes(_sig), _arguments);
		for (auto i: txData)
			gas += i != 0 ? GasCosts::txDataNonZeroGas : GasCosts::txDataZeroGas;
		return gas;
	}

protected:
	map<ASTNode const*, eth::GasMeter::GasConsumption> m_gasCosts;
};

BOOST_FIXTURE_TEST_SUITE(GasMeterTests, GasMeterTestFramework)

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(non_overlapping_filtered_costs, 1)
BOOST_AUTO_TEST_CASE(non_overlapping_filtered_costs)
{
	char const* sourceCode = R"(
		contract test {
			bytes x;
			function f(uint a) returns (uint b) {
				x.length = a;
				for (; a < 200; ++a) {
					x[a] = 9;
					b = a * a;
				}
				return f(a - 1);
			}
		}
	)";
	compile(sourceCode);
	for (auto first = m_gasCosts.cbegin(); first != m_gasCosts.cend(); ++first)
	{
		auto second = first;
		for (++second; second != m_gasCosts.cend(); ++second)
			if (first->first->location().intersects(second->first->location()))
			{
				BOOST_CHECK_MESSAGE(false, "Source locations should not overlap!");
				auto scannerFromSource = [&](string const& _sourceName) -> Scanner const& { return m_compiler.scanner(_sourceName); };
				SourceReferenceFormatter formatter(cout, scannerFromSource);

				formatter.printSourceLocation(&first->first->location());
				formatter.printSourceLocation(&second->first->location());
			}
	}
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(simple_contract, 1)
BOOST_AUTO_TEST_CASE(simple_contract)
{
	// Tests a simple "deploy contract" code without constructor. The actual contract is not relevant.
	char const* sourceCode = R"(
		contract test {
			bytes32 public shaValue;
			function f(uint a) {
				shaValue = keccak256(a);
			}
		}
	)";
	testCreationTimeGas(sourceCode);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(store_keccak256, 1)
BOOST_AUTO_TEST_CASE(store_keccak256)
{
	char const* sourceCode = R"(
		contract test {
			bytes32 public shaValue;
			function test(uint a) {
				shaValue = keccak256(a);
			}
		}
	)";
	testCreationTimeGas(sourceCode);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(updating_store, 1)
BOOST_AUTO_TEST_CASE(updating_store)
{
	char const* sourceCode = R"(
		contract test {
			uint data;
			uint data2;
			function test() {
				data = 1;
				data = 2;
				data2 = 0;
			}
		}
	)";
	testCreationTimeGas(sourceCode);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(branches, 1)
BOOST_AUTO_TEST_CASE(branches)
{
	char const* sourceCode = R"(
		contract test {
			uint data;
			uint data2;
			function f(uint x) {
				if (x > 7)
					data2 = 1;
				else
					data = 1;
			}
		}
	)";
	testCreationTimeGas(sourceCode);
	testRunTimeGas("f(uint256)", vector<vector<bytes>>{encodeArgs(2), encodeArgs(8)});
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(function_calls, 1)
BOOST_AUTO_TEST_CASE(function_calls)
{
	char const* sourceCode = R"(
		contract test {
			uint data;
			uint data2;
			function f(uint x) {
				if (x > 7)
					data2 = g(x**8) + 1;
				else
					data = 1;
			}
			function g(uint x) internal returns (uint) {
				return data2;
			}
		}
	)";
	testCreationTimeGas(sourceCode);
	testRunTimeGas("f(uint256)", vector<vector<bytes>>{encodeArgs(2), encodeArgs(8)});
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(multiple_external_functions, 1)
BOOST_AUTO_TEST_CASE(multiple_external_functions)
{
	char const* sourceCode = R"(
		contract test {
			uint data;
			uint data2;
			function f(uint x) {
				if (x > 7)
					data2 = g(x**8) + 1;
				else
					data = 1;
			}
			function g(uint x) returns (uint) {
				return data2;
			}
		}
	)";
	testCreationTimeGas(sourceCode);
	testRunTimeGas("f(uint256)", vector<vector<bytes>>{encodeArgs(2), encodeArgs(8)});
	testRunTimeGas("g(uint256)", vector<vector<bytes>>{encodeArgs(2)});
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(exponent_size, 1)
BOOST_AUTO_TEST_CASE(exponent_size)
{
	char const* sourceCode = R"(
		contract A {
			function g(uint x) returns (uint) {
				return x ** 0x100;
			}
			function h(uint x) returns (uint) {
				return x ** 0x10000;
			}
		}
	)";
	testCreationTimeGas(sourceCode);
	testRunTimeGas("g(uint256)", vector<vector<bytes>>{encodeArgs(2)});
	testRunTimeGas("h(uint256)", vector<vector<bytes>>{encodeArgs(2)});
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(balance_gas, 1)
BOOST_AUTO_TEST_CASE(balance_gas)
{
	char const* sourceCode = R"(
		contract A {
			function lookup_balance(address a) returns (uint) {
				return a.balance;
			}
		}
	)";
	testCreationTimeGas(sourceCode);
	testRunTimeGas("lookup_balance(address)", vector<vector<bytes>>{encodeArgs(2), encodeArgs(100)});
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(extcodesize_gas, 1)
BOOST_AUTO_TEST_CASE(extcodesize_gas)
{
	char const* sourceCode = R"(
		contract A {
			function f() returns (uint _s) {
				assembly {
					_s := extcodesize(0x30)
				}
			}
		}
	)";
	testCreationTimeGas(sourceCode);
	testRunTimeGas("f()", vector<vector<bytes>>{encodeArgs()});
}

BOOST_AUTO_TEST_SUITE_END()

}
}
}
