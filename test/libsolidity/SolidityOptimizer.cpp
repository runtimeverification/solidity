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
/**
 * @author Christian <c@ethdev.com>
 * @date 2014
 * Tests for the Solidity optimizer.
 */

#include <test/Metadata.h>
#include <test/libsolidity/SolidityExecutionFramework.h>

#include <libevmasm/Instruction.h>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <string>
#include <tuple>
#include <memory>

using namespace std;
using namespace solidity::util;
using namespace solidity::evmasm;
using namespace solidity::test;

namespace solidity::frontend::test
{

class OptimizerTestFramework: public SolidityExecutionFramework
{
public:
	bytes const& compileAndRunWithOptimizer(
		std::string const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		bool const _optimize = true,
		unsigned const _optimizeRuns = 200
	)
	{
		OptimiserSettings previousSettings = std::move(m_optimiserSettings);
		// This uses "none" / "full" while most other test frameworks use
		// "minimal" / "standard".
		m_optimiserSettings = _optimize ? OptimiserSettings::full() : OptimiserSettings::none();
		m_optimiserSettings.expectedExecutionsPerDeployment = _optimizeRuns;
		bytes const& ret = compileAndRun(_sourceCode, _value, _contractName);
		m_optimiserSettings = std::move(previousSettings);
		return ret;
	}

	/// Compiles the source code with and without optimizing.
	void compileBothVersions(
		std::string const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		unsigned const _optimizeRuns = 200
	)
	{
		m_nonOptimizedBytecode = compileAndRunWithOptimizer("pragma solidity >=0.0;\n" + _sourceCode, _value, _contractName, false, _optimizeRuns);
		m_nonOptimizedContract = m_contractAddress;
		m_optimizedBytecode = compileAndRunWithOptimizer("pragma solidity >=0.0;\n" + _sourceCode, _value, _contractName, true, _optimizeRuns);
		size_t nonOptimizedSize = m_nonOptimizedBytecode.size();
		size_t optimizedSize = m_optimizedBytecode.size();
		BOOST_CHECK_MESSAGE(
			_optimizeRuns < 50 || optimizedSize < nonOptimizedSize,
			string("Optimizer did not reduce bytecode size. Non-optimized size: ") +
			to_string(nonOptimizedSize) + " - optimized size: " +
			to_string(optimizedSize)
		);
		m_optimizedContract = m_contractAddress;
	}

	template <class... Args>
	void compareVersions(std::string _sig, Args const&... _arguments)
	{
		m_contractAddress = m_nonOptimizedContract;
		std::vector<bytes> nonOptimizedOutput = callContractFunction(_sig, _arguments...);
		m_gasUsedNonOptimized = m_gasUsed;
		m_contractAddress = m_optimizedContract;
		std::vector<bytes> optimizedOutput = callContractFunction(_sig, _arguments...);
		m_gasUsedOptimized = m_gasUsed;
		BOOST_CHECK_MESSAGE(!optimizedOutput.empty(), "No optimized output for " + _sig);
		BOOST_CHECK_MESSAGE(!nonOptimizedOutput.empty(), "No un-optimized output for " + _sig);
		BOOST_CHECK_MESSAGE(nonOptimizedOutput == optimizedOutput, "Computed values do not match.");
	}

	/// @returns the number of instructions in the given bytecode, not taking the metadata hash
	/// into account.
	size_t numInstructions(bytes const& _bytecode, std::optional<Instruction> _which = std::optional<Instruction>{})
	{
		bytes realCode = bytecodeSansMetadata(_bytecode);
		BOOST_REQUIRE_MESSAGE(!realCode.empty(), "Invalid or missing metadata in bytecode.");
		size_t instructions = 0;
		evmasm::eachInstruction(realCode, [&](Instruction _instr, u256 const&) {
			if (!_which || *_which == _instr)
				instructions++;
		});
		return instructions;
	}

protected:
	u256 m_gasUsedOptimized;
	u256 m_gasUsedNonOptimized;
	bytes m_nonOptimizedBytecode;
	bytes m_optimizedBytecode;
	h160 m_optimizedContract;
	h160 m_nonOptimizedContract;
};

BOOST_FIXTURE_TEST_SUITE(SolidityOptimizer, OptimizerTestFramework)

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(smoke_test, 1)
BOOST_AUTO_TEST_CASE(smoke_test)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint a) public returns (uint b) {
				return a;
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint)", u256(7));
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(identities, 1)
BOOST_AUTO_TEST_CASE(identities)
{
	char const* sourceCode = R"(
		contract test {
			function f(int a) public returns (int b) {
				return int(0) | (int(1) * (int(0) ^ (0 + a)));
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(int)", u256(0x12334664));
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(unused_expressions, 1)
BOOST_AUTO_TEST_CASE(unused_expressions)
{
	char const* sourceCode = R"(
		contract test {
			uint data;
			function f() public returns (uint a, uint b) {
				10 + 20;
				data;
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f()");
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(constant_folding_both_sides, 1)
BOOST_AUTO_TEST_CASE(constant_folding_both_sides)
{
	// if constants involving the same associative and commutative operator are applied from both
	// sides, the operator should be applied only once, because the expression compiler pushes
	// literals as late as possible
	char const* sourceCode = R"(
		contract test {
			function f(uint x) public returns (uint y) {
				return 98 ^ (7 * ((1 | (x | 1000)) * 40) ^ 102);
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint)", 7);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(storage_access, 1)
BOOST_AUTO_TEST_CASE(storage_access)
{
	char const* sourceCode = R"(
		contract test {
			uint8[40] data;
			function f(uint x) public returns (uint y) {
				data[2] = data[7] = uint8(x);
				data[4] = data[2] * 10 + data[3];
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint)", 7);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(array_copy, 1)
BOOST_AUTO_TEST_CASE(array_copy)
{
	char const* sourceCode = R"(
		contract test {
			bytes2[] data1;
			bytes5[] data2;
			function f(uint x, bytes data) public returns (uint l, uint y) {
				data1.length = data.length;
				for (uint i = 0; i < data.length; i++)
					data1.push();
				for (uint i = 0; i < data.length; ++i)
					data1[i] = data[i];
				data2 = data1;
				l = data2.length;
				y = uint(uint40(data2[x]));
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint,bytes)", 0, encodeDyn(string("1234012345678901234567890123456789ab")));
	compareVersions("f(uint,bytes)", 10, encodeDyn(string("1234012345678901234567890123456789ab")));
	compareVersions("f(uint,bytes)", 35, encodeDyn(string("1234012345678901234567890123456789ab")));
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(function_calls, 1)
BOOST_AUTO_TEST_CASE(function_calls)
{
	char const* sourceCode = R"(
		contract test {
			function f1(uint256 x) public returns (uint256) { unchecked { return x*x; } }
			function f(uint256 x) public returns (uint256) { unchecked { return f1(7+x) - this.f1(x**9); } }
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint256)", 0);
	compareVersions("f(uint256)", 10);
	compareVersions("f(uint256)", 36);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(storage_write_in_loops, 1)
BOOST_AUTO_TEST_CASE(storage_write_in_loops)
{
	char const* sourceCode = R"(
		contract test {
			uint d;
			function f(uint a) public returns (uint r) {
				uint x = d;
				for (uint i = 1; i < a * a; i++) {
					r = d;
					d = i;
				}

			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint)", 0);
	compareVersions("f(uint)", 10);
	compareVersions("f(uint)", 36);
}

// Test disabled with https://github.com/ethereum/solidity/pull/762
// Information in joining branches is not retained anymore.
BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(retain_information_in_branches, 1)
BOOST_AUTO_TEST_CASE(retain_information_in_branches)
{
	// This tests that the optimizer knows that we already have "z == keccak256(abi.encodePacked(y))" inside both branches.
	char const* sourceCode = R"(
		contract c {
			bytes32 d;
			uint a;
			function f(uint x, bytes32 y) public returns (uint r_a, bytes32 r_d) {
				bytes32 z = keccak256(abi.encodePacked(y));
				if (x > 8) {
					z = keccak256(abi.encodePacked(y));
					a = x;
				} else {
					z = keccak256(abi.encodePacked(y));
					a = x;
				}
				r_a = a;
				r_d = d;
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint,bytes32)", 0, "abc");
	compareVersions("f(uint,bytes32)", 8, "def");
	compareVersions("f(uint,bytes32)", 10, "ghi");

	bytes optimizedBytecode = compileAndRunWithOptimizer(sourceCode, 0, "c", true);
	size_t numSHA3s = 0;
	eachInstruction(optimizedBytecode, [&](Instruction _instr, u256 const&) {
		if (_instr == Instruction::KECCAK256)
			numSHA3s++;
	});
// TEST DISABLED - OPTIMIZER IS NOT EFFECTIVE ON THIS ONE ANYMORE
//	BOOST_CHECK_EQUAL(1, numSHA3s);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(store_tags_as_unions, 1)
BOOST_AUTO_TEST_CASE(store_tags_as_unions)
{
	// This calls the same function from two sources and both calls have a certain Keccak-256 on
	// the stack at the same position.
	// Without storing tags as unions, the return from the shared function would not know where to
	// jump and thus all jumpdests are forced to clear their state and we do not know about the
	// sha3 anymore.
	// Note that, for now, this only works if the functions have the same number of return
	// parameters since otherwise, the return jump addresses are at different stack positions
	// which triggers the "unknown jump target" situation.
	char const* sourceCode = R"(
		contract test {
			bytes32 data;
			function f(uint x, bytes32 y) external returns (uint r_a, bytes32 r_d) {
				r_d = keccak256(abi.encodePacked(y));
				shared(y);
				r_d = keccak256(abi.encodePacked(y));
				r_a = 5;
			}
			function g(uint x, bytes32 y) external returns (uint r_a, bytes32 r_d) {
				r_d = keccak256(abi.encodePacked(y));
				shared(y);
				r_d = bytes32(uint(keccak256(abi.encodePacked(y))) + 2);
				r_a = 7;
			}
			function shared(bytes32 y) internal {
				data = keccak256(abi.encodePacked(y));
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint,bytes32)", 7, "abc");

	bytes optimizedBytecode = compileAndRunWithOptimizer(sourceCode, 0, "test", true);
	size_t numSHA3s = 0;
	eachInstruction(optimizedBytecode, [&](Instruction _instr, u256 const&) {
		if (_instr == Instruction::KECCAK256)
			numSHA3s++;
	});
// TEST DISABLED UNTIL 93693404 IS IMPLEMENTED
//	BOOST_CHECK_EQUAL(2, numSHA3s);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(incorrect_storage_access_bug, 1)
BOOST_AUTO_TEST_CASE(incorrect_storage_access_bug)
{
	// This bug appeared because a Keccak-256 operation with too low sequence number was used,
	// resulting in memory not being rewritten before the Keccak-256. The fix was to
	// take the max of the min sequence numbers when merging the states.
	char const* sourceCode = R"(
		contract C
		{
			mapping(uint256 => uint) data;
			function f() public returns (uint)
			{
				if (data[block.timestamp] == 0)
					data[type(uint256).max - 6] = 5;
				return data[block.timestamp];
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f()");
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(sequence_number_for_calls, 1)
BOOST_AUTO_TEST_CASE(sequence_number_for_calls)
{
	// This is a test for a bug that was present because we did not increment the sequence
	// number for CALLs - CALLs can read and write from memory (and DELEGATECALLs can do the same
	// to storage), so the sequence number should be incremented.
	char const* sourceCode = R"(
		contract test {
			function f(string memory a, string memory b) public returns (bool) { return sha256(bytes(a)) == sha256(bytes(b)); }
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(string,string)", encodeDyn(string("abc")), encodeDyn(string("def")));
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(computing_constants, 2)
BOOST_AUTO_TEST_CASE(computing_constants)
{
	char const* sourceCode = R"(
		contract C {
			uint m_a;
			uint m_b;
			uint m_c;
			uint m_d;
			constructor() {
				set();
			}
			function set() public returns (uint) {
				m_a = 0x77abc0000000000000000000000000000000000000000000000000000000001;
				m_b = 0x817416927846239487123469187231298734162934871263941234127518276;
				g();
				return 1;
			}
			function g() public {
				m_b = 0x817416927846239487123469187231298734162934871263941234127518276;
				m_c = 0x817416927846239487123469187231298734162934871263941234127518276;
				h();
			}
			function h() public {
				m_d = 0xff05694900000000000000000000000000000000000000000000000000000000;
			}
			function get() public returns (uint ra, uint rb, uint rc, uint rd) {
				ra = m_a;
				rb = m_b;
				rc = m_c;
				rd = m_d;
			}
		}
	)";
	compileBothVersions(sourceCode, 0, "C", 1);
	compareVersions("get()");
	compareVersions("set()");
	compareVersions("get()");

	bytes optimizedBytecode = compileAndRunWithOptimizer(sourceCode, 0, "C", true, 1);
	bytes complicatedConstant = toBigEndian(u256("0x817416927846239487123469187231298734162934871263941234127518276"));
	unsigned occurrences = 0;
	for (auto iter = optimizedBytecode.cbegin(); iter < optimizedBytecode.cend(); ++occurrences)
	{
		iter = search(iter, optimizedBytecode.cend(), complicatedConstant.cbegin(), complicatedConstant.cend());
		if (iter < optimizedBytecode.cend())
			++iter;
	}
	BOOST_CHECK_EQUAL(2, occurrences);

	bytes constantWithZeros = toBigEndian(u256("0x77abc0000000000000000000000000000000000000000000000000000000001"));
	BOOST_CHECK(search(
		optimizedBytecode.cbegin(),
		optimizedBytecode.cend(),
		constantWithZeros.cbegin(),
		constantWithZeros.cend()
	) == optimizedBytecode.cend());
}


BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(constant_optimization_early_exit, 1)
BOOST_AUTO_TEST_CASE(constant_optimization_early_exit)
{
	// This tests that the constant optimizer does not try to find the best representation
	// indefinitely but instead stops after some number of iterations.
	char const* sourceCode = R"(
	contract HexEncoding {
		function hexEncodeTest(address addr) public returns (bytes32 ret) {
			uint256 x = uint256(uint160(addr)) / 2**32;

			// Nibble interleave
			x = x & 0x00000000000000000000000000000000ffffffffffffffffffffffffffffffff;
			x = (x | (x * 2**64)) & 0x0000000000000000ffffffffffffffff0000000000000000ffffffffffffffff;
			x = (x | (x * 2**32)) & 0x00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff;
			x = (x | (x * 2**16)) & 0x0000ffff0000ffff0000ffff0000ffff0000ffff0000ffff0000ffff0000ffff;
			x = (x | (x * 2** 8)) & 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff;
			x = (x | (x * 2** 4)) & 0x0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f;

			// Hex encode
			uint256 h = (x & 0x0808080808080808080808080808080808080808080808080808080808080808) / 8;
			uint256 i = (x & 0x0404040404040404040404040404040404040404040404040404040404040404) / 4;
			uint256 j = (x & 0x0202020202020202020202020202020202020202020202020202020202020202) / 2;
			x = x + (h & (i | j)) * 0x27 + 0x3030303030303030303030303030303030303030303030303030303030303030;

			uint256[2] memory arr;
			arr[0] = x;
			x = uint160(addr) * 2**96;

			// Nibble interleave
			x = x & 0x00000000000000000000000000000000ffffffffffffffffffffffffffffffff;
			x = (x | (x * 2**64)) & 0x0000000000000000ffffffffffffffff0000000000000000ffffffffffffffff;
			x = (x | (x * 2**32)) & 0x00000000ffffffff00000000ffffffff00000000ffffffff00000000ffffffff;
			x = (x | (x * 2**16)) & 0x0000ffff0000ffff0000ffff0000ffff0000ffff0000ffff0000ffff0000ffff;
			x = (x | (x * 2** 8)) & 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff;
			x = (x | (x * 2** 4)) & 0x0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f;

			// Hex encode
			h = (x & 0x0808080808080808080808080808080808080808080808080808080808080808) / 8;
			i = (x & 0x0404040404040404040404040404040404040404040404040404040404040404) / 4;
			j = (x & 0x0202020202020202020202020202020202020202020202020202020202020202) / 2;
			x = x + (h & (i | j)) * 0x27 + 0x3030303030303030303030303030303030303030303030303030303030303030;

			// Store and hash
			arr[1] = x;
			return keccak256(arr);
		}
	}
	)";
	auto start = std::chrono::steady_clock::now();
	compileBothVersions(sourceCode);
	double duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	// Since run time on an ASan build is not really realistic, we disable this test for those builds.
	size_t maxDuration = 20;
#if !defined(__SANITIZE_ADDRESS__) && defined(__has_feature)
#if __has_feature(address_sanitizer)
#define __SANITIZE_ADDRESS__ 1
#endif
#endif
#if __SANITIZE_ADDRESS__
	maxDuration = numeric_limits<size_t>::max();
	BOOST_TEST_MESSAGE("Disabled constant optimizer run time check for address sanitizer build.");
#endif
	BOOST_CHECK_MESSAGE(duration <= double(maxDuration), "Compilation of constants took longer than 20 seconds.");
	compareVersions("hexEncodeTest(address)", u256(0x123456789));
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(inconsistency, 1)
BOOST_AUTO_TEST_CASE(inconsistency)
{
	// This is a test of a bug in the optimizer.
	char const* sourceCode = R"(
		contract Inconsistency {
			struct Value {
				uint badnum;
				uint number;
			}

			struct Container {
				uint[] valueIndices;
				Value[] values;
			}

			Container[] containers;
			uint[] valueIndices;
			uint INDEX_ZERO = 0;
			uint  debug;

			// Called with params: containerIndex=0, valueIndex=0
			function levelIII(uint containerIndex, uint valueIndex) private {
				Container storage container = containers[containerIndex];
				Value storage value = container.values[valueIndex];
				debug = container.valueIndices[value.number];
			}
			function levelII() private {
				for (uint i = 0; i < valueIndices.length; i++) {
					levelIII(INDEX_ZERO, valueIndices[i]);
				}
			}

			function trigger() public returns (uint) {
				Container storage container = containers.push();

				container.values.push(Value({
					badnum: 9000,
					number: 0
				}));

				container.valueIndices.push();
				valueIndices.push();

				levelII();
				return debug;
			}

			function DoNotCallButDoNotDelete() public {
				levelII();
				levelIII(1, 2);
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("trigger()");
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(dead_code_elimination_across_assemblies, 1)
BOOST_AUTO_TEST_CASE(dead_code_elimination_across_assemblies)
{
	// This tests that a runtime-function that is stored in storage in the constructor
	// is not removed as part of dead code elimination.
	char const* sourceCode = R"(
		contract DCE {
			function () internal returns (uint) stored;
			constructor() {
				stored = f;
			}
			function f() internal returns (uint) { return 7; }
			function test() public returns (uint) { return stored(); }
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("test()");
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(invalid_state_at_control_flow_join, 1)
BOOST_AUTO_TEST_CASE(invalid_state_at_control_flow_join)
{
	char const* sourceCode = R"(
		contract Test {
			uint public totalSupply = 100;
			function f() public returns (uint r) {
				if (false)
					r = totalSupply;
				totalSupply -= 10;
			}
			function test() public returns (uint) {
				f();
				return this.totalSupply();
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("test()");
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(init_empty_dynamic_arrays, 2)
BOOST_AUTO_TEST_CASE(init_empty_dynamic_arrays)
{
	// This is not so much an optimizer test, but rather a test
	// that allocating empty arrays is implemented efficiently.
	// In particular, initializing a dynamic memory array does
	// not use any memory.
	char const* sourceCode = R"(
		contract Test {
			function f() public pure returns (uint r) {
				uint[][] memory x = new uint[][](20000);
				return x.length;
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f()");
	BOOST_CHECK_LE(m_gasUsedNonOptimized, 1900000);
	BOOST_CHECK_LE(1600000, m_gasUsedNonOptimized);
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(optimise_multi_stores, 3)
BOOST_AUTO_TEST_CASE(optimise_multi_stores)
{
	char const* sourceCode = R"(
		contract Test {
			struct S { uint16 a; uint16 b; uint16[3] c; uint[] dyn; }
			uint padding;
			S[] s;
			function f() public returns (uint16, uint16, uint16[3] memory, uint) {
				uint16[3] memory c;
				c[0] = 7;
				c[1] = 8;
				c[2] = 9;
				s.push(S(1, 2, c, new uint[](4)));
				return (s[0].a, s[0].b, s[0].c, s[0].dyn[2]);
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f()");
	BOOST_CHECK_EQUAL(numInstructions(m_nonOptimizedBytecode, Instruction::SSTORE), 8);
	BOOST_CHECK_EQUAL(numInstructions(m_optimizedBytecode, Instruction::SSTORE), 7);
}

BOOST_AUTO_TEST_CASE(optimise_constant_to_codecopy)
{
	char const* sourceCode = R"(
		contract C {
			// We use the state variable so that the functions won't be deemed identical
			// and be optimised out to the same implementation.
			uint a;
			function f() public returns (uint) {
				a = 1;
				// This cannot be represented well with the `CalculateMethod`,
				// hence the decision will be between `LiteralMethod` and `CopyMethod`.
				return 0x1234123412431234123412412342112341234124312341234124;
			}
			function g() public returns (uint) {
				a = 2;
				return 0x1234123412431234123412412342112341234124312341234124;
			}
			function h() public returns (uint) {
				a = 3;
				return 0x1234123412431234123412412342112341234124312341234124;
			}
			function i() public returns (uint) {
				a = 4;
				return 0x1234123412431234123412412342112341234124312341234124;
			}
		}
	)";
	compileBothVersions(sourceCode, 0, "C", 50);
	compareVersions("f()");
	compareVersions("g()");
	compareVersions("h()");
	compareVersions("i()");
	// This is counting in the deployed code.
	BOOST_CHECK_EQUAL(numInstructions(m_nonOptimizedBytecode, Instruction::CODECOPY), 0);
	BOOST_CHECK_EQUAL(numInstructions(m_optimizedBytecode, Instruction::CODECOPY), 4);
}

BOOST_AUTO_TEST_CASE(byte_access)
{
	char const* sourceCode = R"(
		contract C
		{
			function f(bytes32 x) public returns (bytes1 r)
			{
				assembly { r := and(byte(x, 31), 0xff) }
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(bytes32)", u256("0x1223344556677889900112233445566778899001122334455667788990011223"));
}

BOOST_AUTO_TEST_CASE(shift_optimizer_bug)
{
	char const* sourceCode = R"(
		contract C
		{
			function f(uint x) public returns (uint)
			{
				return (x << 1) << type(uint).max;
			}
			function g(uint x) public returns (uint)
			{
				return (x >> 1) >> type(uint).max;
			}
		}
	)";
	compileBothVersions(sourceCode);
	compareVersions("f(uint256)", 7);
	compareVersions("g(uint256)", u256(-1));
}

BOOST_AUTO_TEST_CASE(avoid_double_cleanup)
{
	char const* sourceCode = R"(
		contract C {
			receive() external payable {
				abi.encodePacked(uint200(0));
			}
		}
	)";
	compileBothVersions(sourceCode, 0, "C", 50);
	// Check that there is no double AND instruction in the resulting code
	BOOST_CHECK_EQUAL(numInstructions(m_nonOptimizedBytecode, Instruction::AND), 1);
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
