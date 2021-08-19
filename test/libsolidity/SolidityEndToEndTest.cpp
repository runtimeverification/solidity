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
 * @author Gav Wood <g@ethdev.com>
 * @date 2014
 * Unit tests for the solidity expression compiler, testing the behaviour of the code.
 */

#include <test/libsolidity/SolidityExecutionFramework.h>

#include <test/Common.h>
#include <test/EVMHost.h>

#include <liblangutil/Exceptions.h>
#include <liblangutil/EVMVersion.h>

#include <libevmasm/Assembly.h>

#include <libsolutil/Keccak256.h>
#include <libsolutil/ErrorCodes.h>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/test/unit_test.hpp>

#include <functional>
#include <numeric>
#include <string>
#include <tuple>

using namespace std;
using namespace std::placeholders;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::test;
using namespace solidity::langutil;

#define ALSO_VIA_YUL(CODE) { CODE }
/*
#define ALSO_VIA_YUL(CODE)                      \
{                                               \
	m_doEwasmTestrun = true;                    \
                                                \
	m_compileViaYul = false;                    \
	m_compileToEwasm = false;                   \
	{ CODE }                                    \
                                                \
	m_compileViaYul = true;                     \
	reset();                                    \
	{ CODE }                                    \
                                                \
	if (m_doEwasmTestrun)                       \
	{                                           \
		m_compileToEwasm = true;                \
		reset();                                \
		{ CODE }                                \
	}                                           \
}
*/

#define DISABLE_EWASM_TESTRUN()
/*
#define DISABLE_EWASM_TESTRUN() \
	{ m_doEwasmTestrun = false; }
*/

namespace solidity::frontend::test
{

struct SolidityEndToEndTestExecutionFramework: public SolidityExecutionFramework
{
	bool m_doEwasmTestrun = false;
};

BOOST_FIXTURE_TEST_SUITE(SolidityEndToEndTest, SolidityEndToEndTestExecutionFramework)

unsigned constexpr roundTo32(unsigned _num)
{
	return (_num + 31) / 32 * 32;
}

BOOST_AUTO_TEST_CASE(encode_negative_int)
{
  char const* sourceCode = R"(
    contract ReturnIntArray {
      function returnIntArray() public pure returns (int[3] memory) {
        int[3] memory x;
        x[0] = -1;
        x[1] = -255;
        x[2] = -128;
        return x;
      }
    }
  )";
  compileAndRun(sourceCode);
  ABI_CHECK(callContractFunction("returnIntArray()"), vector<bytes>(1, encodeRefArgs(
		bigint(-1), bigint(-255), bigint(-128) // element values
	)));
}

BOOST_AUTO_TEST_CASE(exp_operator)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint a) public returns(uint d) { return 2 ** a; }
		}
	)";
	compileAndRun(sourceCode);
	testContractAgainstCppOnRange("f(uint)", [](u256 const& a) -> u256 { return u256(1 << a.convert_to<int>()); }, 0, 16);
}

BOOST_AUTO_TEST_CASE(exp_zero)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint a) public returns(uint d) { return a ** 0; }
		}
	)";
	compileAndRun(sourceCode);
	testContractAgainstCppOnRange("f(uint)", [](u256 const&) -> u256 { return u256(1); }, 0, 16);
}

/* TODO let's add this back when I figure out the correct type conversion.
BOOST_AUTO_TEST_CASE(conditional_expression_string_literal)
{
	char const* sourceCode = R"(
		contract test {
			function f(bool cond) public returns (bytes32) {
				return cond ? "true" : "false";
			}
		}
	)";
	compileAndRun(sourceCode);
	ABI_CHECK(callContractFunction("f(bool)", true), encodeArgs(string("true", 4)));
	ABI_CHECK(callContractFunction("f(bool)", false), encodeArgs(string("false", 5)));
}
*/

BOOST_AUTO_TEST_CASE(C99_scoping_activation_loops)
{
  char const* sourceCode = R"(
    contract test {
      function f(uint n) pure public returns (uint x1, uint x2) {
        for (uint i = 1; i <= n; i++) {
          uint y;
          x1 = x1 + y + i;
          y = i;
          x2 = x2 + y + i;
        }
      }
    }
  )";
  compileAndRun(sourceCode);
  ABI_CHECK(callContractFunction("f(uint)", encodeArgs(3)), encodeArgs(6, 12));
}

BOOST_AUTO_TEST_CASE(recursive_calls)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint n) public returns(uint nfac) {
				if (n <= 1) return 1;
				else return n * f(n - 1);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		function<u256(u256)> recursive_calls_cpp = [&recursive_calls_cpp](u256 const& n) -> u256
		{
			if (n <= 1)
				return 1;
			else
				return n * recursive_calls_cpp(n - 1);
		};

		testContractAgainstCppOnRange("f(uint)", recursive_calls_cpp, 0, 5);
	)
}

BOOST_AUTO_TEST_CASE(while_loop)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint n) public returns(uint nfac) {
				nfac = 1;
				uint i = 2;
				while (i <= n) nfac *= i++;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto while_loop_cpp = [](u256 const& n) -> u256
		{
			u256 nfac = 1;
			u256 i = 2;
			while (i <= n)
				nfac *= i++;

			return nfac;
		};

		testContractAgainstCppOnRange("f(uint)", while_loop_cpp, 0, 5);
	)
}

BOOST_AUTO_TEST_CASE(do_while_loop)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint n) public returns(uint nfac) {
				nfac = 1;
				uint i = 2;
				do { nfac *= i++; } while (i <= n);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto do_while_loop_cpp = [](u256 const& n) -> u256
		{
			u256 nfac = 1;
			u256 i = 2;
			do
			{
				nfac *= i++;
			}
			while (i <= n);

			return nfac;
		};

		testContractAgainstCppOnRange("f(uint)", do_while_loop_cpp, 0, 5);
	)
}

BOOST_AUTO_TEST_CASE(do_while_loop_multiple_local_vars)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint x) public pure returns(uint r) {
				uint i = 0;
				do
				{
					uint z = x * 2;
					if (z < 4) break;
					else {
						uint k = z + 1;
						if (k < 8) {
							x++;
							continue;
						}
					}
					if (z > 12) return 0;
					x++;
					i++;
				} while (true);
				return 42;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto do_while = [](u256 n) -> u256
		{
			u256 i = 0;
			do
			{
				u256 z = n * 2;
				if (z < 4) break;
				else {
					u256 k = z + 1;
					if (k < 8) {
						n++;
						continue;
					}
				}
				if (z > 12) return 0;
				n++;
				i++;
			} while (true);
			return 42;
		};

		testContractAgainstCppOnRange("f(uint)", do_while, 0, 12);
	)
}

BOOST_AUTO_TEST_CASE(nested_loops)
{
	// tests that break and continue statements in nested loops jump to the correct place
	char const* sourceCode = R"(
		contract test {
			function f(uint x) public returns(uint y) {
				while (x > 1) {
					if (x == 10) break;
					while (x > 5) {
						if (x == 8) break;
						x--;
						if (x == 6) continue;
						return x;
					}
					x--;
					if (x == 3) continue;
					break;
				}
				return x;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto nested_loops_cpp = [](u256 n) -> u256
		{
			while (n > 1)
			{
				if (n == 10)
					break;
				while (n > 5)
				{
					if (n == 8)
						break;
					n--;
					if (n == 6)
						continue;
					return n;
				}
				n--;
				if (n == 3)
					continue;
				break;
			}

			return n;
		};

		testContractAgainstCppOnRange("f(uint)", nested_loops_cpp, 0, 12);
	)
}

BOOST_AUTO_TEST_CASE(nested_loops_multiple_local_vars)
{
	// tests that break and continue statements in nested loops jump to the correct place
	// and free local variables properly
	char const* sourceCode = R"(
		contract test {
			function f(uint x) public returns(uint y) {
				while (x > 0) {
					uint z = x + 10;
					uint k = z + 1;
					if (k > 20) {
						break;
						uint p = 100;
						k += p;
					}
					if (k > 15) {
						x--;
						continue;
						uint t = 1000;
						x += t;
					}
					while (k > 10) {
						uint m = k - 1;
						if (m == 10) return x;
						return k;
						uint h = 10000;
						z += h;
					}
					x--;
					break;
				}
				return x;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto nested_loops_cpp = [](u256 n) -> u256
		{
			while (n > 0)
			{
				u256 z = n + 10;
				u256 k = z + 1;
				if (k > 20) break;
				if (k > 15) {
					n--;
					continue;
				}
				while (k > 10)
				{
					u256 m = k - 1;
					if (m == 10) return n;
					return k;
				}
				n--;
				break;
			}

			return n;
		};

		testContractAgainstCppOnRange("f(uint)", nested_loops_cpp, 0, 12);
	)
}

BOOST_AUTO_TEST_CASE(for_loop_multiple_local_vars)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint x) public pure returns(uint r) {
				for (uint i = 0; i < 12; i++)
				{
					uint z = x + 1;
					if (z < 4) break;
					else {
						uint k = z * 2;
						if (i + k < 10) {
							x++;
							continue;
						}
					}
					if (z > 8) return 0;
					x++;
				}
				return 42;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto for_loop = [](u256 n) -> u256
		{
			for (u256 i = 0; i < 12; i++)
			{
				u256 z = n + 1;
				if (z < 4) break;
				else {
					u256 k = z * 2;
					if (i + k < 10) {
						n++;
						continue;
					}
				}
				if (z > 8) return 0;
				n++;
			}
			return 42;
		};

		testContractAgainstCppOnRange("f(uint)", for_loop, 0, 12);
	)
}

BOOST_AUTO_TEST_CASE(nested_for_loop_multiple_local_vars)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint x) public pure returns(uint r) {
				for (uint i = 0; i < 5; i++)
				{
					uint z = x + 1;
					if (z < 3) {
						break;
						uint p = z + 2;
					}
					for (uint j = 0; j < 5; j++)
					{
						uint k = z * 2;
						if (j + k < 8) {
							x++;
							continue;
							uint t = z * 3;
						}
						x++;
						if (x > 20) {
							return 84;
							uint h = x + 42;
						}
					}
					if (x > 30) {
						return 42;
						uint b = 0xcafe;
					}
				}
				return 42;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto for_loop = [](u256 n) -> u256
		{
			for (u256 i = 0; i < 5; i++)
			{
				u256 z = n + 1;
				if (z < 3) break;
				for (u256 j = 0; j < 5; j++)
				{
					u256 k = z * 2;
					if (j + k < 8) {
						n++;
						continue;
					}
					n++;
					if (n > 20) return 84;
				}
				if (n > 30) return 42;
			}
			return 42;
		};

		testContractAgainstCppOnRange("f(uint)", for_loop, 0, 12);
	)
}

BOOST_AUTO_TEST_CASE(for_loop)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint n) public returns(uint nfac) {
				nfac = 1;
				uint i;
				for (i = 2; i <= n; i++)
					nfac *= i;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto for_loop_cpp = [](u256 const& n) -> u256
		{
			u256 nfac = 1;
			for (auto i = 2; i <= n; i++)
				nfac *= i;
			return nfac;
		};

		testContractAgainstCppOnRange("f(uint)", for_loop_cpp, 0, 5);
	)
}

BOOST_AUTO_TEST_CASE(for_loop_simple_init_expr)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint n) public returns(uint nfac) {
				nfac = 1;
				uint256 i;
				for (i = 2; i <= n; i++)
					nfac *= i;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto for_loop_simple_init_expr_cpp = [](u256 const& n) -> u256
		{
			u256 nfac = 1;
			u256 i;
			for (i = 2; i <= n; i++)
				nfac *= i;
			return nfac;
		};

		testContractAgainstCppOnRange("f(uint)", for_loop_simple_init_expr_cpp, 0, 5);
	)
}

BOOST_AUTO_TEST_CASE(for_loop_break_continue)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint n) public returns (uint r)
			{
				uint i = 1;
				uint k = 0;
				for (i *= 5; k < n; i *= 7)
				{
					k++;
					i += 4;
					if (n % 3 == 0)
						break;
					i += 9;
					if (n % 2 == 0)
						continue;
					i += 19;
				}
				return i;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);

		auto breakContinue = [](u256 const& n) -> u256
		{
			u256 i = 1;
			u256 k = 0;
			for (i *= 5; k < n; i *= 7)
			{
				k++;
				i += 4;
				if (n % 3 == 0)
					break;
				i += 9;
				if (n % 2 == 0)
					continue;
				i += 19;
			}
			return i;
		};

		testContractAgainstCppOnRange("f(uint)", breakContinue, 0, 10);
	);
}

BOOST_AUTO_TEST_CASE(short_circuiting)
{
	char const* sourceCode = R"(
		contract test {
			function run(uint x) public returns(uint y) {
				x == 0 || ((x = 8) > 0);
				return x;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		auto short_circuiting_cpp = [](u256 n) -> u256
		{
			(void)(n == 0 || (n = 8) > 0);
			return n;
		};

		testContractAgainstCppOnRange("run(uint)", short_circuiting_cpp, 0, 2);
	)
}

BOOST_AUTO_TEST_CASE(high_bits_cleaning)
{
	char const* sourceCode = R"(
		contract test {
			function run() public returns(uint y) {
				unchecked {
					uint32 t = uint32(0xffffffff);
					uint32 x = t + 10;
					if (x >= 0xffffffff) return 0;
					return x;
				}
			}
		}
	)";
	compileAndRun(sourceCode);
	auto high_bits_cleaning_cpp = []() -> u256
	{
		uint32_t t = uint32_t(0xffffffff);
		uint32_t x = t + 10;
		if (x >= 0xffffffff)
			return 0;
		return x;
	};
	testContractAgainstCpp("run()", high_bits_cleaning_cpp);
}

BOOST_AUTO_TEST_CASE(sign_extension)
{
	char const* sourceCode = R"(
		contract test {
			function run() public returns(uint256 y) {
				unchecked {
					int64 x = -int32(int64(0xff));
					if (x >= 0xff) return 0;
					return 0 - uint256(int256(x));
				}
			}
		}
	)";
	compileAndRun(sourceCode);
	auto sign_extension_cpp = []() -> u256
	{
		int64_t x = -int32_t(0xff);
		if (x >= 0xff)
			return 0;
		return u256(x) * -1;
	};
	testContractAgainstCpp("run()", sign_extension_cpp);
}

BOOST_AUTO_TEST_CASE(small_unsigned_types)
{
	char const* sourceCode = R"(
		contract test {
			function run() public returns(uint y) {
				unchecked {
					uint32 t = uint32(0xffffff);
					uint32 x = t * 0xffffff;
					return x / 0x100;
				}
			}
		}
	)";
	compileAndRun(sourceCode);
	auto small_unsigned_types_cpp = []() -> u256
	{
		uint32_t t = uint32_t(0xffffff);
		uint32_t x = t * 0xffffff;
		return x / 0x100;
	};
	testContractAgainstCpp("run()", small_unsigned_types_cpp);
}

BOOST_AUTO_TEST_CASE(mapping_state_inc_dec)
{
	char const* sourceCode = R"(
		contract test {
			uint256 value;
			mapping(uint256 => uint256) table;
			function f(uint256 x) public returns (uint256 y) {
				value = x;
				if (x > 0) table[++value] = 8;
				if (x > 1) value--;
				if (x > 2) table[value]++;
				table[value] += 10;
				return --table[value++];
			}
		}
	)";

	u256 value;
	map<u256, u256> table;
	auto f = [&](u256 const& _x) -> u256
	{
		value = _x;
		if (_x > 0)
			table[++value] = 8;
		if (_x > 1)
			value --;
		if (_x > 2)
			table[value]++;
		table[value] += 10;
		return --table[value++];
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		value = 0;
		table.clear();

		testContractAgainstCppOnRange("f(uint256)", f, 0, 5);
	)
}

BOOST_AUTO_TEST_CASE(multi_level_mapping)
{
	char const* sourceCode = R"(
		contract test {
			mapping(uint => mapping(uint => uint)) table;
			function f(uint x, uint y, uint z) public returns (uint w) {
				if (z == 0) return table[x][y];
				else return table[x][y] = z;
			}
		}
	)";
	map<u256, map<u256, u256>> table;
	auto f = [&](u256 const& _x, u256 const& _y, u256 const& _z) -> u256
	{
		if (_z == 0) return table[_x][_y];
		else return table[_x][_y] = _z;
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		table.clear();

		testContractAgainstCpp("f(uint,uint,uint)", f, u256(4), u256(5), u256(0));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(5), u256(4), u256(0));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(4), u256(5), u256(9));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(4), u256(5), u256(0));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(5), u256(4), u256(0));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(5), u256(4), u256(7));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(4), u256(5), u256(0));
		testContractAgainstCpp("f(uint,uint,uint)", f, u256(5), u256(4), u256(0));
	)
}

BOOST_AUTO_TEST_CASE(constructor)
{
	char const* sourceCode = R"(
		contract test {
			mapping(uint => uint) data;
			constructor() {
				data[7] = 8;
			}
			function get(uint key) public returns (uint value) {
				return data[key];
			}
		}
	)";

	map<u256, uint8_t> data;
	data[7] = 8;
	auto get = [&](u256 const& _x) -> u256
	{
		return data[_x];
	};

	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		testContractAgainstCpp("get(uint)", get, u256(6));
		testContractAgainstCpp("get(uint)", get, u256(7));
	)
}

BOOST_AUTO_TEST_CASE(send_ether)
{
	char const* sourceCode = R"(
		contract test {
			constructor() payable {}
			function a(address payable addr, uint amount) public returns (uint ret) {
				addr.send(amount);
				return address(this).balance;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		u256 amount(250);
		compileAndRun(sourceCode, amount + 1);
		h160 address(23);
		ABI_CHECK(callContractFunction("a(address,uint)", address, amount), encodeArgs(1));
		BOOST_CHECK_EQUAL(balanceAt(address), amount);
	)
}

BOOST_AUTO_TEST_CASE(transfer_ether)
{
	char const* sourceCode = R"(
		contract A {
			constructor() payable {}
			function a(address payable addr, uint amount) public returns (uint) {
				addr.transfer(amount);
				return address(this).balance;
			}
			function b(address payable addr, uint amount) public {
				addr.transfer(amount);
			}
		}

		contract B {
		}

		contract C {
			receive () external payable {
				revert();
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode, 0, "B");
		h160 const nonPayableRecipient = m_contractAddress;
		compileAndRun(sourceCode, 0, "C");
		h160 const oogRecipient = m_contractAddress;
		compileAndRun(sourceCode, 20, "A");
		h160 payableRecipient(23);
		ABI_CHECK(callContractFunction("a(address,uint)", payableRecipient, 10), encodeArgs(10));
		BOOST_CHECK_EQUAL(balanceAt(payableRecipient), 10);
		BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 10);
		ABI_CHECK(callContractFunction("b(address,uint)", nonPayableRecipient, 10), encodeArgs());
		ABI_CHECK(callContractFunction("b(address,uint)", oogRecipient, 10), encodeArgs());
	)
}

BOOST_AUTO_TEST_CASE(selfdestruct)
{
	char const* sourceCode = R"(
		contract test {
			constructor() payable {}
			function a(address payable receiver) public returns (uint ret) {
				selfdestruct(receiver);
				return 10;
			}
		}
	)";
	u256 amount(130);
	h160 address(23);
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode, amount);
		ABI_CHECK(callContractFunction("a(address)", address), vector<bytes>());
		BOOST_CHECK(!addressHasCode(m_contractAddress));
		BOOST_CHECK_EQUAL(balanceAt(address), amount);
	)
}

BOOST_AUTO_TEST_CASE(keccak256)
{
	char const* sourceCode = R"(
		contract test {
			function a(bytes32 input) public returns (bytes32 hash) {
				return keccak256(abi.encodePacked(input));
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		auto f = [&](u256 const& _x) -> u256
		{
			return util::keccak256(util::toBigEndian(_x));
		};
		testContractAgainstCpp("a(bytes32)", f, u256(4));
		testContractAgainstCpp("a(bytes32)", f, u256(5));
		testContractAgainstCpp("a(bytes32)", f, u256(-1));
	);
}

BOOST_AUTO_TEST_CASE(sha256)
{
	char const* sourceCode = R"(
		contract test {
			function a(bytes32 input) public returns (bytes32 sha256hash) {
				return sha256(abi.encodePacked(input));
			}
		}
	)";
	auto f = [&](u256 const& _x) -> bytes
	{
		if (_x == u256(4))
			return fromHex("00e38990d0c7fc009880a9c07c23842e886c6bbdc964ce6bdd5817ad357335ee6f");
		if (_x == u256(5))
			return fromHex("0096de8fc8c256fa1e1556d41af431cace7dca68707c78dd88c3acab8b17164c47");
		if (_x == u256(-1))
			return fromHex("00af9613760f72635fbdb44a5a0a63c39f12af30f950a6ee5c971be188e89c4051");
		return fromHex("");
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		testContractAgainstCpp("a(bytes32)", f, u256(4));
		testContractAgainstCpp("a(bytes32)", f, u256(5));
		testContractAgainstCpp("a(bytes32)", f, u256(-1));
	)
}

BOOST_AUTO_TEST_CASE(ripemd)
{
	char const* sourceCode = R"(
		contract test {
			function a(bytes32 input) public returns (bytes32 sha256hash) {
				return ripemd160(abi.encodePacked(input));
			}
		}
	)";
	auto f = [&](u256 const& _x) -> bytes
	{
		if (_x == u256(4))
			return fromHex("1b0f3c404d12075c68c938f9f60ebea4f74941a0000000000000000000000000");
		if (_x == u256(5))
			return fromHex("00ee54aa84fc32d8fed5a5fe160442ae84626829d9000000000000000000000000");
		if (_x == u256(-1))
			return fromHex("1cf4e77f5966e13e109703cd8a0df7ceda7f3dc3000000000000000000000000");
		return fromHex("");
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		testContractAgainstCpp("a(bytes32)", f, u256(4));
		testContractAgainstCpp("a(bytes32)", f, u256(5));
		testContractAgainstCpp("a(bytes32)", f, u256(-1));
	)
}

BOOST_AUTO_TEST_CASE(packed_keccak256)
{
	char const* sourceCode = R"(
		contract test {
			function a(bytes32 input) public returns (bytes32 hash) {
				uint24 b = 65536;
				uint c = 256;
				return keccak256(abi.encodePacked(uint8(8), input, b, input, c));
			}
		}
	)";
	auto f = [&](u256 const& _x) -> u256
	{
		return util::keccak256(
			toCompactBigEndian(unsigned(8)) +
			util::toBigEndian(_x) +
			toCompactBigEndian(unsigned(65536)) +
			util::toBigEndian(_x) +
			toBigEndian(u256(256))
		);
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		testContractAgainstCpp("a(bytes32)", f, u256(4));
		testContractAgainstCpp("a(bytes32)", f, u256(5));
		testContractAgainstCpp("a(bytes32)", f, u256(-1));
	)
}

BOOST_AUTO_TEST_CASE(packed_keccak256_complex_types)
{
	char const* sourceCode = R"(
		contract test {
			uint120[3] x;
			function f() public returns (bytes32 hash1, bytes32 hash2, bytes32 hash3) {
				uint120[] memory y = new uint120[](3);
				x[0] = y[0] = uint120(type(uint256).max - 1);
				x[1] = y[1] = uint120(type(uint256).max - 2);
				x[2] = y[2] = uint120(type(uint256).max - 3);
				hash1 = keccak256(abi.encodePacked(x));
				hash2 = keccak256(abi.encodePacked(y));
				hash3 = keccak256(abi.encodePacked(this.f));
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		// Strangely, arrays are encoded with intra-element padding.
		ABI_CHECK(callContractFunction("f()"), encodeArgs(
			util::keccak256(fromHex("fffffffffffffffffffffffffffffe" "fffffffffffffffffffffffffffffd" "fffffffffffffffffffffffffffffc")),
			util::keccak256(fromHex("fffffffffffffffffffffffffffffe" "fffffffffffffffffffffffffffffd" "fffffffffffffffffffffffffffffc")),
			util::keccak256(fromHex(m_contractAddress.hex() + "0001"))
		));
	)
}

BOOST_AUTO_TEST_CASE(packed_sha256)
{
	char const* sourceCode = R"(
		contract test {
			function a(bytes32 input) public returns (bytes32 hash) {
				uint24 b = 65536;
				uint256 c = 256;
				return sha256(abi.encodePacked(uint8(8), input, b, input, c));
			}
		}
	)";
	auto f = [&](u256 const& _x) -> bytes
	{
		if (_x == u256(4))
			return fromHex("00804e0d7003cfd70fc925dc103174d9f898ebb142ecc2a286da1abd22ac2ce3ac");
		if (_x == u256(5))
			return fromHex("00e94921945f9068726c529a290a954f412bcac53184bb41224208a31edbf63cf0");
		if (_x == u256(-1))
			return fromHex("00f14def4d07cd185ddd8b10a81b2238326196a38867e6e6adbcc956dc913488c7");
		return fromHex("");
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		testContractAgainstCpp("a(bytes32)", f, u256(4));
		testContractAgainstCpp("a(bytes32)", f, u256(5));
		testContractAgainstCpp("a(bytes32)", f, u256(-1));
	)
}

BOOST_AUTO_TEST_CASE(packed_ripemd160)
{
	char const* sourceCode = R"(
		contract test {
			function a(bytes32 input) public returns (bytes32 hash) {
				uint24 b = 65536;
				uint256 c = 256;
				return ripemd160(abi.encodePacked(uint8(8), input, b, input, c));
			}
		}
	)";
	auto f = [&](u256 const& _x) -> bytes
	{
		if (_x == u256(4))
			return fromHex("00f93175303eba2a7b372174fc9330237f5ad202fc000000000000000000000000");
		if (_x == u256(5))
			return fromHex("04f4fc112e2bfbe0d38f896a46629e08e2fcfad5000000000000000000000000");
		if (_x == u256(-1))
			return fromHex("00c0a2e4b1f3ff766a9a0089e7a410391730872495000000000000000000000000");
		return fromHex("");
	};
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		testContractAgainstCpp("a(bytes32)", f, u256(4));
		testContractAgainstCpp("a(bytes32)", f, u256(5));
		testContractAgainstCpp("a(bytes32)", f, u256(-1));
	)
}

BOOST_AUTO_TEST_CASE(inter_contract_calls)
{
	char const* sourceCode = R"(
		contract Helper {
			function multiply(uint a, uint b) public returns (uint c) {
				return a * b;
			}
		}
		contract Main {
			Helper h;
			function callHelper(uint a, uint b) public returns (uint c) {
				return h.multiply(a, b);
			}
			function getHelper() public returns (address haddress) {
				return address(h);
			}
			function setHelper(address haddress) public {
				h = Helper(haddress);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	u160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 0, "Main");
	BOOST_REQUIRE(callContractFunction("setHelper(address)", c_helperAddress) == vector<bytes>());
	BOOST_REQUIRE(callContractFunction("getHelper()") == encodeArgs(c_helperAddress));
	u256 a(3456789);
	u256 b("0x282837623374623234aa74");
	BOOST_REQUIRE(callContractFunction("callHelper(uint,uint)", a, b) == encodeArgs(a * b));
}

BOOST_AUTO_TEST_CASE(inter_contract_calls_with_complex_parameters)
{
	char const* sourceCode = R"(
		contract Helper {
			function sel(uint a, bool select, uint b) public returns (uint c) {
				if (select) return a; else return b;
			}
		}
		contract Main {
			Helper h;
			function callHelper(uint a, bool select, uint b) public returns (uint c) {
				return h.sel(a, select, b) * 3;
			}
			function getHelper() public returns (address haddress) {
				return address(h);
			}
			function setHelper(address haddress) public {
				h = Helper(haddress);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	u160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 0, "Main");
	BOOST_REQUIRE(callContractFunction("setHelper(address)", c_helperAddress) == vector<bytes>());
	BOOST_REQUIRE(callContractFunction("getHelper()") == encodeArgs(c_helperAddress));
	u256 a(3456789);
	u256 b("0x282837623374623234aa74");
	BOOST_REQUIRE(callContractFunction("callHelper(uint,bool,uint)", a, true, b) == encodeArgs(a * 3));
	BOOST_REQUIRE(callContractFunction("callHelper(uint,bool,uint)", a, false, b) == encodeArgs(b * 3));
}

BOOST_AUTO_TEST_CASE(inter_contract_calls_accessing_this)
{
	char const* sourceCode = R"(
		contract Helper {
			function getAddress() public returns (address addr) {
				return address(this);
			}
		}
		contract Main {
			Helper h;
			function callHelper() public returns (address addr) {
				return h.getAddress();
			}
			function getHelper() public returns (address addr) {
				return address(h);
			}
			function setHelper(address addr) public {
				h = Helper(addr);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	u160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 0, "Main");
	BOOST_REQUIRE(callContractFunction("setHelper(address)", c_helperAddress) == vector<bytes>());
	BOOST_REQUIRE(callContractFunction("getHelper()") == encodeArgs(c_helperAddress));
	BOOST_REQUIRE(callContractFunction("callHelper()") == encodeArgs(c_helperAddress));
}

BOOST_AUTO_TEST_CASE(calls_to_this)
{
	char const* sourceCode = R"(
		contract Helper {
			function invoke(uint a, uint b) public returns (uint c) {
				return this.multiply(a, b, 10);
			}
			function multiply(uint a, uint b, uint8 c) public returns (uint ret) {
				return a * b + c;
			}
		}
		contract Main {
			Helper h;
			function callHelper(uint a, uint b) public returns (uint ret) {
				return h.invoke(a, b);
			}
			function getHelper() public returns (address addr) {
				return address(h);
			}
			function setHelper(address addr) public {
				h = Helper(addr);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	u160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 0, "Main");
	BOOST_REQUIRE(callContractFunction("setHelper(address)", c_helperAddress) == vector<bytes>());
	BOOST_REQUIRE(callContractFunction("getHelper()") == encodeArgs(c_helperAddress));
	u256 a(3456789);
	u256 b("0x282837623374623234aa74");
	BOOST_REQUIRE(callContractFunction("callHelper(uint,uint)", a, b) == encodeArgs(a * b + 10));
}

BOOST_AUTO_TEST_CASE(inter_contract_calls_with_local_vars)
{
	// note that a reference to another contract's function occupies two stack slots,
	// so this tests correct stack slot allocation
	char const* sourceCode = R"(
		contract Helper {
			function multiply(uint a, uint b) public returns (uint c) {
				return a * b;
			}
		}
		contract Main {
			Helper h;
			function callHelper(uint a, uint b) public returns (uint c) {
				uint8 y = 9;
				uint ret = h.multiply(a, b);
				return ret + y;
			}
			function getHelper() public returns (address haddress) {
				return address(h);
			}
			function setHelper(address haddress) public {
				h = Helper(haddress);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	u160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 0, "Main");
	BOOST_REQUIRE(callContractFunction("setHelper(address)", c_helperAddress) == vector<bytes>());
	BOOST_REQUIRE(callContractFunction("getHelper()") == encodeArgs(c_helperAddress));
	u256 a(3456789);
	u256 b("0x282837623374623234aa74");
	BOOST_REQUIRE(callContractFunction("callHelper(uint,uint)", a, b) == encodeArgs(a * b + 9));
}

BOOST_AUTO_TEST_CASE(fixed_bytes_in_calls)
{
	char const* sourceCode = R"(
		contract Helper {
			function invoke(bytes3 x, bool stop) public returns (bytes4 ret) {
				return x;
			}
		}
		contract Main {
			Helper h;
			function callHelper(bytes2 x, bool stop) public returns (bytes5 ret) {
				return h.invoke(x, stop);
			}
			function getHelper() public returns (address addr) {
				return address(h);
			}
			function setHelper(address addr) public {
				h = Helper(addr);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	u160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 0, "Main");
	BOOST_REQUIRE(callContractFunction("setHelper(address)", c_helperAddress) == vector<bytes>());
	BOOST_REQUIRE(callContractFunction("getHelper()") == encodeArgs(c_helperAddress));
	ABI_CHECK(callContractFunction("callHelper(bytes2,bool)", string("\0a\0", 3), true), encodeArgs(string("a\0\0\0\0", 5)));
}

BOOST_AUTO_TEST_CASE(constructor_with_long_arguments)
{
	char const* sourceCode = R"(
		contract Main {
			string public a;
			string public b;

			constructor(string memory _a, string memory _b) {
				a = _a;
				b = _b;
			}
		}
	)";
	string a = "01234567890123gabddunaouhdaoneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi45678907890123456789abcd123456787890123456789abcd90123456789012345678901234567890123456789aboneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi45678907890123456789abcd123456787890123456789abcd90123456789012345678901234567890123456789aboneudapcgadi4567890789012oneudapcgadi4567890789012oneudapcgadi45678907890123456789abcd123456787890123456789abcd90123456789012345678901234567890123456789aboneudapcgadi4567890789012cdef";
	string b = "AUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PYAUTAHIACIANOTUHAOCUHAOEUNAOEHUNTHDYDHPYDRCPYDRSTITOEUBXHUDGO>PY";

	compileAndRun(sourceCode, 0, "Main", encodeArgs(
		encodeRefArgs(uint64_t(a.length()),
		a),
		encodeRefArgs(uint64_t(b.length()),
		b)
	));
	ABI_CHECK(callContractFunction("a()"), vector<bytes>(1, encodeDyn(a)));
	ABI_CHECK(callContractFunction("b()"), vector<bytes>(1, encodeDyn(b)));
}

BOOST_AUTO_TEST_CASE(contracts_as_addresses)
{
	char const* sourceCode = R"(
		contract helper {
			receive() external payable { } // can receive ether
		}
		contract test {
			helper h;
			constructor() payable { h = new helper(); payable(h).send(5); }
			function getBalance() public returns (uint myBalance, uint helperBalance) {
				myBalance = address(this).balance;
				helperBalance = address(h).balance;
			}
		}
	)";
	compileAndRun(sourceCode, 20);
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 20 - 5);
	BOOST_REQUIRE(callContractFunction("getBalance()") == encodeArgs(u256(20 - 5), u256(5)));
}

BOOST_AUTO_TEST_CASE(blockhash)
{
	char const* sourceCode = R"(
		contract C {
			uint256 counter;
			function g() public returns (bool) { counter++; return true; }
			function f() public returns (bytes32[] memory r) {
				r = new bytes32[](259);
				for (uint256 i = 0; i < 259; i++) {
					unchecked {
						r[i] = blockhash(block.number - 257 + i);
					}
				}
			}
		}
	)";
	compileAndRun(sourceCode);
	// generate a sufficient amount of blocks
	while (blockNumber() < u256(255))
		ABI_CHECK(callContractFunction("g()"), encodeArgs(true));

	vector<u256> hashes;
	// ``blockhash()`` is only valid for the last 256 blocks, otherwise zero
	hashes.emplace_back(0);
	for (u256 i = blockNumber() - u256(255); i <= blockNumber(); i++)
		hashes.emplace_back(blockHash(i));
	// the current block hash is not yet known at execution time and therefore zero
	hashes.emplace_back(0);
	// future block hashes are zero
	hashes.emplace_back(0);

	ABI_CHECK(callContractFunction("f()"), vector<bytes>(1, encodeDyn(hashes)));
}

BOOST_AUTO_TEST_CASE(internal_constructor)
{
	char const* sourceCode = R"(
		abstract contract C {
			constructor() {}
		}
	)";
	compileAndRunWithoutCheck({{"", sourceCode}}, 0, "C");
	BOOST_CHECK(m_status != 0);
}

BOOST_AUTO_TEST_CASE(default_fallback_throws)
{
	char const* sourceCode = R"YY(
		contract A {
		}
	)YY";
	compileAndRun(sourceCode);
	ABI_CHECK(callFallback(), encodeArgs());
	BOOST_CHECK_EQUAL(m_status, 1);
}

BOOST_AUTO_TEST_CASE(event)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit(address indexed _from, bytes32 indexed _id, uint _value);
			function deposit(bytes32 _id, bool _manually) public payable {
				//if (_manually) {
				//	bytes32 s = 0xba4147ba0b861a26291c6fc19ebe604a0d4bf7d01d477cc5d9e10691704b7983;
				//	uint value = msg.value;
				//	address sender = msg.sender;
				//	assembly {
				//		mstore(0, value)
				//		log3(0, 0x20, s, sender, _id)
				//	}
				//} else {
					emit Deposit(msg.sender, _id, msg.value);
				//}
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		u256 value(18);
		u256 id(0x1234);
		for (bool manually: {true, false})
		{
			callContractFunctionWithValue("deposit(bytes32,bool)", value, id, manually);
			BOOST_REQUIRE_EQUAL(numLogs(), 1);
			BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
			BOOST_CHECK_EQUAL(h256(logData(0), h256::AlignLeft), h256(encodeLogs(h256(u256(value)))));
			BOOST_REQUIRE_EQUAL(numLogTopics(0), 3);
			BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,bytes32,uint)")));
			BOOST_CHECK_EQUAL(logTopic(0, 1), h256(m_sender, h256::AlignRight));
			BOOST_CHECK_EQUAL(logTopic(0, 2), h256(id));
		}
	)
}

BOOST_AUTO_TEST_CASE(event_emit)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit(address indexed _from, bytes32 indexed _id, uint _value);
			function deposit(bytes32 _id) public payable {
				emit Deposit(msg.sender, _id, msg.value);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		u256 value(18);
		u256 id(0x1234);
		callContractFunctionWithValue("deposit(bytes32)", value, id);
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK_EQUAL(h256(logData(0), h256::AlignRight), h256(u256(value)));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 3);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,bytes32,uint)")));
		BOOST_CHECK_EQUAL(logTopic(0, 1), h256(m_sender, h256::AlignRight));
		BOOST_CHECK_EQUAL(logTopic(0, 2), h256(id));
	)
}

BOOST_AUTO_TEST_CASE(event_constructor)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit(address indexed _from, bytes32 indexed _id, uint _value);
			constructor() {
				emit Deposit(msg.sender, bytes32("abc"), 7);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK_EQUAL(h256(logData(0), h256::AlignRight), h256(u256(7)));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 3);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,bytes32,uint)")));
		BOOST_CHECK_EQUAL(logTopic(0, 1), h256(m_sender, h256::AlignRight));
		BOOST_CHECK_EQUAL(logTopic(0, 2), h256(string{"abc"}, h256::FromBinary, h256::AlignLeft));
	)
}


BOOST_AUTO_TEST_CASE(event_no_arguments)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit();
			function deposit() public {
				emit Deposit();
			}
		}
	)";

	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		callContractFunction("deposit()");
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0).empty());
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit()")));
	)
}

BOOST_AUTO_TEST_CASE(event_access_through_base_name_emit)
{
	char const* sourceCode = R"(
		contract A {
			event x();
		}
		contract B is A {
			function f() public returns (uint) {
				emit A.x();
				return 1;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		callContractFunction("f()");
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0).empty());
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("x()")));
	);
}

BOOST_AUTO_TEST_CASE(events_with_same_name)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit();
			event Deposit(address _addr);
			event Deposit(address _addr, uint _amount);
			event Deposit(address _addr, bool _flag);
			function deposit() public returns (uint) {
				emit Deposit();
				return 1;
			}
			function deposit(address _addr) public returns (uint) {
				emit Deposit(_addr);
				return 2;
			}
			function deposit(address _addr, uint _amount) public returns (uint) {
				emit Deposit(_addr, _amount);
				return 3;
			}
			function deposit(address _addr, bool _flag) public returns (uint) {
				emit Deposit(_addr, _flag);
				return 4;
			}
		}
	)";
	h160 const c_loggedAddress = m_contractAddress;

	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction("deposit()"), encodeArgs(u256(1)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0).empty());
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit()")));

		ABI_CHECK(callContractFunction("deposit(address)", c_loggedAddress), encodeArgs(u256(2)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		ABI_CHECK(logData(0), encodeLog(c_loggedAddress));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address)")));

		ABI_CHECK(callContractFunction("deposit(address,uint)", c_loggedAddress, u256(100)), encodeArgs(u256(3)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		ABI_CHECK(logData(0), encodeLogs(c_loggedAddress, uint64_t(1), 100));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,uint)")));

		ABI_CHECK(callContractFunction("deposit(address,bool)", c_loggedAddress, false), encodeArgs(u256(4)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		ABI_CHECK(logData(0), encodeLogs(c_loggedAddress, false));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,bool)")));
	)
}

BOOST_AUTO_TEST_CASE(events_with_same_name_inherited_emit)
{
	char const* sourceCode = R"(
		contract A {
			event Deposit();
		}

		contract B {
			event Deposit(address _addr);
		}

		contract ClientReceipt is A, B {
			event Deposit(address _addr, uint _amount);
			function deposit() public returns (uint) {
				emit Deposit();
				return 1;
			}
			function deposit(address _addr) public returns (uint) {
				emit Deposit(_addr);
				return 1;
			}
			function deposit(address _addr, uint _amount) public returns (uint) {
				emit Deposit(_addr, _amount);
				return 1;
			}
		}
	)";
	h160 const c_loggedAddress = m_contractAddress;

	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction("deposit()"), encodeArgs(u256(1)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0).empty());
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit()")));

		ABI_CHECK(callContractFunction("deposit(address)", c_loggedAddress), encodeArgs(u256(1)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == encodeLog(c_loggedAddress));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address)")));

		ABI_CHECK(callContractFunction("deposit(address,uint)", c_loggedAddress, u256(100)), encodeArgs(u256(1)));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		ABI_CHECK(logData(0), encodeLogs(c_loggedAddress, uint64_t(1), 100));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,uint)")));
	)
}

BOOST_AUTO_TEST_CASE(event_anonymous)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit() anonymous;
			function deposit() public {
				emit Deposit();
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		callContractFunction("deposit()");
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 0);
	)
}

BOOST_AUTO_TEST_CASE(event_anonymous_with_topics)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit(address indexed _from, bytes32 indexed _id, uint indexed _value, uint indexed _value2, bytes32 data) anonymous;
			function deposit(bytes32 _id) public payable {
				emit Deposit(msg.sender, _id, msg.value, 2, "abc");
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		u256 value(18);
		u256 id(0x1234);
		callContractFunctionWithValue("deposit(bytes32)", value, id);
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == encodeLogs("abc" + string(29, 0)));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 4);
		BOOST_CHECK_EQUAL(logTopic(0, 0), h256(m_sender, h256::AlignRight));
		BOOST_CHECK_EQUAL(logTopic(0, 1), h256(id));
		BOOST_CHECK_EQUAL(logTopic(0, 2), h256(value));
		BOOST_CHECK_EQUAL(logTopic(0, 3), h256(2));
	)
}

BOOST_AUTO_TEST_CASE(event_lots_of_data)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit(address _from, bytes32 _id, uint _value, bool _flag);
			function deposit(bytes32 _id) public payable {
				emit Deposit(msg.sender, _id, msg.value, true);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		int value = 18;
		u256 id(0x1234);
		callContractFunctionWithValue("deposit(bytes32)", value, id);
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == encodeLogs(m_sender, id, uint64_t(1), value, true));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,bytes32,uint,bool)")));
	)
}

BOOST_AUTO_TEST_CASE(event_really_lots_of_data)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			event Deposit(uint fixeda, bytes dynx, uint fixedb);
			function deposit(bytes memory data) public {
				emit Deposit(10, data, 15);
			}
		}
	)";
	compileAndRun(sourceCode);
	bytes data = FixedHash<4>(util::keccak256("deposit(bytes)")).asBytes();
	callContractFunction("deposit(bytes)", encodeRefArgs(uint64_t(4), data));
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK_EQUAL(toHex(logData(0)), toHex(encodeLogs(bigint(10), uint64_t(4), data, bigint(15))));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(uint,bytes,uint)")));
}

BOOST_AUTO_TEST_CASE(event_really_lots_of_data_from_storage)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			bytes x;
			event Deposit(uint fixeda, bytes dynx, uint fixedb);
			function deposit() public {
				x.push("A");
				x.push("B");
				x.push("C");
				emit Deposit(10, x, 15);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("deposit()");
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK_EQUAL(toHex(logData(0)), toHex(encodeLogs(bigint(10), uint64_t(3), string("ABC"), bigint(15))));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(uint,bytes,uint)")));
}

BOOST_AUTO_TEST_CASE(event_really_really_lots_of_data_from_storage)
{
	char const* sourceCode = R"(
		contract ClientReceipt {
			bytes x;
			event Deposit(uint fixeda, bytes dynx, uint fixedb);
			function deposit() public {
				x = new bytes(31);
				x[0] = "A";
				x[1] = "B";
				x[2] = "C";
				x[30] = "Z";
				emit Deposit(10, x, 15);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("deposit()");
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(bigint(10), uint64_t(31), string("ABC") + string(27, 0) + "Z", bigint(15)));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(uint,bytes,uint)")));
}

BOOST_AUTO_TEST_CASE(event_struct_memory_v2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			struct S { uint256 a; }
			event E(S);
			function createEvent(uint256 x) public {
				emit E(S(x));
			}
		}
	)";
	compileAndRun(sourceCode);
	u256 x(42);
	callContractFunction("createEvent(uint256)", x);
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(x));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E((uint256))")));
}

BOOST_AUTO_TEST_CASE(event_struct_storage_v2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			struct S { uint256 a; }
			event E(S);
			S s;
			function createEvent(uint256 x) public {
				s.a = x;
				emit E(s);
			}
		}
	)";
	compileAndRun(sourceCode);
	u256 x(42);
	callContractFunction("createEvent(uint256)", x);
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(x));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E((uint256))")));
}

BOOST_AUTO_TEST_CASE(event_dynamic_array_memory)
{
	char const* sourceCode = R"(
		contract C {
			event E(uint256[]);
			function createEvent(uint256 x) public {
				uint256[] memory arr = new uint256[](3);
				arr[0] = x;
				arr[1] = x + 1;
				arr[2] = x + 2;
				emit E(arr);
			}
		}
	)";
	compileAndRun(sourceCode);
	u256 x(42);
	callContractFunction("createEvent(uint256)", x);
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(1, 3, x, x + 1, x + 2));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(uint256[])")));
}

BOOST_AUTO_TEST_CASE(event_dynamic_array_memory_v2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			event E(uint256[]);
			function createEvent(uint256 x) public {
				uint256[] memory arr = new uint256[](3);
				arr[0] = x;
				arr[1] = x + 1;
				arr[2] = x + 2;
				emit E(arr);
			}
		}
	)";
	compileAndRun(sourceCode);
	u256 x(42);
	callContractFunction("createEvent(uint256)", x);
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(1, 3, x, x + 1, x + 2));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(uint256[])")));
}

BOOST_AUTO_TEST_CASE(event_dynamic_nested_array_memory_v2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			event E(uint256[][]);
			function createEvent(uint256 x) public {
				uint256[][] memory arr = new uint256[][](2);
				arr[0] = new uint256[](2);
				arr[1] = new uint256[](2);
				arr[0][0] = x;
				arr[0][1] = x + 1;
				arr[1][0] = x + 2;
				arr[1][1] = x + 3;
				emit E(arr);
			}
		}
	)";
	compileAndRun(sourceCode);
	u256 x(42);
	callContractFunction("createEvent(uint256)", x);
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(1, 2, 1, 2, x, x + 1, 1, 2, x + 2, x + 3));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(uint256[][])")));
}

BOOST_AUTO_TEST_CASE(event_dynamic_array_storage)
{
	char const* sourceCode = R"(
		contract C {
			event E(uint256[]);
			uint256[] arr;
			function createEvent(uint256 x) public {
				while (arr.length < 3)
					arr.push();
				arr[0] = x;
				arr[1] = x + 1;
				arr[2] = x + 2;
				emit E(arr);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		u256 x(42);
		callContractFunction("createEvent(uint256)", x);
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == encodeLogs(1, 3, x, x + 1, x + 2));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(uint256[])")));
	)
}

BOOST_AUTO_TEST_CASE(event_dynamic_array_storage_v2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			event E(uint256[]);
			uint256[] arr;
			function createEvent(uint256 x) public {
				while (arr.length < 3)
					arr.push();
				arr[0] = x;
				arr[1] = x + 1;
				arr[2] = x + 2;
				emit E(arr);
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		u256 x(42);
		callContractFunction("createEvent(uint256)", x);
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == encodeLogs(1, 3, x, x + 1, x + 2));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(uint256[])")));
	)
}

BOOST_AUTO_TEST_CASE(event_dynamic_nested_array_storage_v2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			event E(uint256[][]);
			uint256[][] arr;
			function createEvent(uint256 x) public {
				arr.push(new uint256[](2));
				arr.push(new uint256[](2));
				arr[0][0] = x;
				arr[0][1] = x + 1;
				arr[1][0] = x + 2;
				arr[1][1] = x + 3;
				emit E(arr);
			}
		}
	)";
	/// TODO enable again after push(..) via yul is implemented for nested arrays.
	/// ALSO_VIA_YUL()
	compileAndRun(sourceCode);
	u256 x(42);
	callContractFunction("createEvent(uint256)", x);
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_CHECK(logData(0) == encodeLogs(1, 2, 1, 2, x, x + 1, 1, 2, x + 2, x + 3));
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(uint256[][])")));
}

BOOST_AUTO_TEST_CASE(event_indexed_string)
{
	char const* sourceCode = R"(
		contract C {
			string x;
			uint[4] y;
			event E(string indexed r, uint[4] indexed t);
			function deposit() public {
				for (uint i = 0; i < 90; i++)
					bytes(x).push(0);
				for (uint8 i = 0; i < 90; i++)
					bytes(x)[i] = bytes1(i);
				y[0] = 4;
				y[1] = 5;
				y[2] = 6;
				y[3] = 7;
				emit E(x, y);
			}
		}
	)";
	compileAndRun(sourceCode);
	callContractFunction("deposit()");
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	string dynx(90, 0);
	std::iota(dynx.begin(), dynx.end(), 0);
	BOOST_CHECK(logData(0) == bytes());
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 3);
	BOOST_CHECK_EQUAL(logTopic(0, 1), util::keccak256(dynx));
	BOOST_CHECK_EQUAL(logTopic(0, 2), util::keccak256(
		encodeLogs(4, 5, 6, 7)
	));
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(string,uint[4])")));
}

BOOST_AUTO_TEST_CASE(event_indexed_function)
{
	char const* sourceCode = R"(
		contract C {
			event Test(function() external indexed);
			function f() public {
				emit Test(this.f);
			}
		}
	)";

	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		callContractFunction("f()");
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == bytes());
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 3);

		BOOST_CHECK_EQUAL(logTopic(0, 1), h256(m_contractAddress, h256::AlignRight));
		BOOST_CHECK_EQUAL(logTopic(0, 2), h256(u256(1)));
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Test(function)")));
	)
}

BOOST_AUTO_TEST_CASE(empty_name_input_parameter_with_named_one)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint, uint k) public returns(uint ret_k, uint ret_g){
				uint g = 8;
				ret_k = k;
				ret_g = g;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		BOOST_CHECK(callContractFunction("f(uint,uint)", 5, 9) != encodeArgs(5, 8));
		ABI_CHECK(callContractFunction("f(uint,uint)", 5, 9), encodeArgs(9, 8));
	)
}

BOOST_AUTO_TEST_CASE(generic_call)
{
	char const* sourceCode = R"**(
			contract receiver {
				uint public received;
				function recv(uint x) public payable { received = x; }
			}
			contract sender {
				constructor() payable {}
				function doSend(address rec) public returns (uint d)
				{
					bytes4 signature = bytes4(bytes32(keccak256("recv(uint)")));
					rec.call{value: 2}(abi.encodeWithSelector(signature, 23));
					return receiver(rec).received();
				}
			}
	)**";
	if (false) {
		compileAndRun(sourceCode, 0, "receiver");
		h160 const c_receiverAddress = m_contractAddress;
		compileAndRun(sourceCode, 50, "sender");
		BOOST_REQUIRE(callContractFunction("doSend(address)", c_receiverAddress) == encodeArgs(23));
		BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 50 - 2);
	}
}

BOOST_AUTO_TEST_CASE(generic_delegatecall)
{
	char const* sourceCode = R"**(
			contract Receiver {
				uint public received;
				address public sender;
				uint public value;
				constructor() payable {}
				function recv(uint x) public payable { received = x; sender = msg.sender; value = msg.value; }
			}
			contract Sender {
				uint public received;
				address public sender;
				uint public value;
				constructor() payable {}
				function doSend(address rec) public payable
				{
					bytes4 signature = bytes4(bytes32(keccak256("recv(uint)")));
					(bool success,) = rec.delegatecall(abi.encodeWithSelector(signature, 23));
					success;
				}
			}
	)**";

	for (auto v2: {false, true})
	{
		string source = "pragma abicoder " + string(v2 ? "v2" : "v1") + ";\n" + string(sourceCode);

		if (false) {
			compileAndRun(sourceCode, 0, "Receiver");
			h160 const c_receiverAddress = m_contractAddress;
			compileAndRun(sourceCode, 50, "Sender");
			h160 const c_senderAddress = m_contractAddress;
			BOOST_CHECK(m_sender != c_senderAddress); // just for sanity
			ABI_CHECK(callContractFunctionWithValue("doSend(address)", 11, c_receiverAddress), encodeArgs());
			ABI_CHECK(callContractFunction("received()"), encodeArgs(u256(23)));
			ABI_CHECK(callContractFunction("sender()"), encodeArgs(m_sender));
			ABI_CHECK(callContractFunction("value()"), encodeArgs(u256(11)));
			m_contractAddress = c_receiverAddress;
			ABI_CHECK(callContractFunction("received()"), encodeArgs(u256(0)));
			ABI_CHECK(callContractFunction("sender()"), encodeArgs(u256(0)));
			ABI_CHECK(callContractFunction("value()"), encodeArgs(u256(0)));
			BOOST_CHECK(storageEmpty(c_receiverAddress));
			BOOST_CHECK(!storageEmpty(c_senderAddress));
			BOOST_CHECK_EQUAL(balanceAt(c_receiverAddress), 0);
			BOOST_CHECK_EQUAL(balanceAt(c_senderAddress), 50 + 11);
		}
	}
}

BOOST_AUTO_TEST_CASE(generic_staticcall)
{
	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
	{
		char const* sourceCode = R"**(
				contract A {
					uint public x;
					constructor() { x = 42; }
					function pureFunction(uint256 p) public pure returns (uint256) { return p; }
					function viewFunction(uint256 p) public view returns (uint256) { return p + x; }
					function nonpayableFunction(uint256 p) public returns (uint256) { x = p; return x; }
					function assertFunction(uint256 p) public view returns (uint256) { assert(x == p); return x; }
				}
				contract C {
					function f(address a) public view returns (bool, bytes memory)
					{
						return a.staticcall(abi.encodeWithSignature("pureFunction(uint256)", 23));
					}
					function g(address a) public view returns (bool, bytes memory)
					{
						return a.staticcall(abi.encodeWithSignature("viewFunction(uint256)", 23));
					}
					function h(address a) public view returns (bool, bytes memory)
					{
						return a.staticcall(abi.encodeWithSignature("nonpayableFunction(uint256)", 23));
					}
					function i(address a, uint256 v) public view returns (bool, bytes memory)
					{
						return a.staticcall(abi.encodeWithSignature("assertFunction(uint256)", v));
					}
				}
		)**";
		if (false) {
			compileAndRun(sourceCode, 0, "A");
			h160 const c_addressA = m_contractAddress;
			compileAndRun(sourceCode, 0, "C");
			ABI_CHECK(callContractFunction("f(address)", c_addressA), encodeArgs(true, 0x40, 0x20, 23));
			ABI_CHECK(callContractFunction("g(address)", c_addressA), encodeArgs(true, 0x40, 0x20, 23 + 42));
			ABI_CHECK(callContractFunction("h(address)", c_addressA), encodeArgs(false, 0x40, 0x00));
			ABI_CHECK(callContractFunction("i(address,uint256)", c_addressA, 42), encodeArgs(true, 0x40, 0x20, 42));
			ABI_CHECK(callContractFunction("i(address,uint256)", c_addressA, 23), encodeArgs(false, 0x40, 0x24) + vector<bytes>(1, panicData(PanicCode::Assert) + bytes(32 - 4, 0)));
		}
	}
}

BOOST_AUTO_TEST_CASE(library_call_in_homestead)
{
	char const* sourceCode = R"(
		library Lib { function m() public returns (address) { return msg.sender; } }
		contract Test {
			address public sender;
			function f() public {
				sender = Lib.m();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs());
	ABI_CHECK(callContractFunction("sender()"), encodeArgs(u160(m_sender)));
}

BOOST_AUTO_TEST_CASE(library_call_protection)
{
	// This tests code that reverts a call if it is a direct call to a library
	// as opposed to a delegatecall.
	char const* sourceCode = R"(
		library Lib {
			struct S { uint x; }
			// a direct call to this should revert
			function np(S storage s) public returns (address) { s.x = 3; return msg.sender; }
			// a direct call to this is fine
			function v(S storage) public view returns (address) { return msg.sender; }
			// a direct call to this is fine
			function pu() public pure returns (uint) { return 2; }
		}
		contract Test {
			Lib.S public s;
			function np() public returns (address) { return Lib.np(s); }
			function v() public view returns (address) { return Lib.v(s); }
			function pu() public pure returns (uint) { return Lib.pu(); }
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	ABI_CHECK(callContractFunction("np(Lib.S storage)", 0), encodeArgs());
	ABI_CHECK(callContractFunction("v(Lib.S storage)", 0), encodeArgs(u160(m_sender)));
	ABI_CHECK(callContractFunction("pu()"), encodeArgs(2));
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("s()"), encodeArgs(0));
	ABI_CHECK(callContractFunction("np()"), encodeArgs(u160(m_sender)));
	ABI_CHECK(callContractFunction("s()"), encodeArgs(3));
	ABI_CHECK(callContractFunction("v()"), encodeArgs(u160(m_sender)));
	ABI_CHECK(callContractFunction("pu()"), encodeArgs(2));
}

BOOST_AUTO_TEST_CASE(library_staticcall_delegatecall)
{
	char const* sourceCode = R"(
		 library Lib {
			 function x() public view returns (uint) {
				 return 1;
			 }
		 }
		 contract Test {
			 uint t;
			 function f() public returns (uint) {
				 t = 2;
				 return this.g();
			 }
			 function g() public view returns (uint) {
				 return Lib.x();
			 }
		 }
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(1));
}

BOOST_AUTO_TEST_CASE(bytes_from_calldata_to_memory)
{
	char const* sourceCode = R"(
		contract C {
			function f(bytes memory data) public returns (bytes32) {
				return keccak256(abi.encodePacked("abc", data));
			}
		}
	)";
	compileAndRun(sourceCode);
	bytes calldata1 = bytes(61, 0x22) + bytes(12, 0x12);
	callContractFunctionNoEncoding("f(bytes)", vector<bytes>{encodeDyn(bytes(61, 0x22) + bytes(12, 0x12))});
	BOOST_CHECK(m_transactionSuccessful);
	BOOST_CHECK(m_output == encodeArgs(util::keccak256(bytes{'a', 'b', 'c'} + calldata1)));
}

BOOST_AUTO_TEST_CASE(call_forward_bytes)
{
	char const* sourceCode = R"(
		contract receiver {
			uint public received;
			function recv(uint x) public { received += x + 1; }
			fallback() external { received = 0x80; }
		}
		contract sender {
			constructor() { rec = new receiver(); }
			fallback() external { savedData = msg.data; }
			function forward() public returns (bool) { address(rec).call(savedData); return true; }
			function clear() public returns (bool) { delete savedData; return true; }
			function val() public returns (uint) { return rec.received(); }
			receiver rec;
			bytes savedData;
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "sender");
		ABI_CHECK(callContractFunction("recv(uint256)", 7), vector<bytes>());
		ABI_CHECK(callContractFunction("val()"), encodeArgs(0));
		ABI_CHECK(callContractFunction("forward()"), encodeArgs(true));
		ABI_CHECK(callContractFunction("val()"), encodeArgs(8));
		ABI_CHECK(callContractFunction("clear()"), encodeArgs(true));
		ABI_CHECK(callContractFunction("val()"), encodeArgs(8));
		ABI_CHECK(callContractFunction("forward()"), encodeArgs(true));
		ABI_CHECK(callContractFunction("val()"), encodeArgs(0x80));
	}
}

BOOST_AUTO_TEST_CASE(call_forward_bytes_length)
{
	char const* sourceCode = R"(
		contract receiver {
			uint public calledLength;
			fallback() external { calledLength = msg.data.length; }
		}
		contract sender {
			receiver rec;
			constructor() { rec = new receiver(); }
			function viaCalldata() public returns (uint) {
				(bool success,) = address(rec).call(msg.data);
				require(success);
				return rec.calledLength();
			}
			function viaMemory() public returns (uint) {
				bytes memory x = msg.data;
				(bool success,) = address(rec).call(x);
				require(success);
				return rec.calledLength();
			}
			bytes s;
			function viaStorage() public returns (uint) {
				s = msg.data;
				(bool success,) = address(rec).call(s);
				require(success);
				return rec.calledLength();
			}
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "sender");

		// No additional data, just function selector
		ABI_CHECK(callContractFunction("viaCalldata()"), encodeArgs(4));
		ABI_CHECK(callContractFunction("viaMemory()"), encodeArgs(4));
		ABI_CHECK(callContractFunction("viaStorage()"), encodeArgs(4));

		// Some additional unpadded data
		vector<bytes> unpadded = vector<bytes>(1, asBytes(string("abc")));
		ABI_CHECK(callContractFunctionNoEncoding("viaCalldata()", unpadded), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("viaMemory()", unpadded), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("viaStorage()", unpadded), encodeArgs(7));
	}
}

BOOST_AUTO_TEST_CASE(copying_bytes_multiassign)
{
	char const* sourceCode = R"(
		contract receiver {
			uint public received;
			function recv(uint x) public { received += x + 1; }
			fallback() external { received = 0x80; }
		}
		contract sender {
			constructor() { rec = new receiver(); }
			fallback() external { savedData1 = savedData2 = msg.data; }
			function forward(bool selector) public returns (bool) {
				if (selector) { address(rec).call(savedData1); delete savedData1; }
				else { address(rec).call(savedData2); delete savedData2; }
				return true;
			}
			function val() public returns (uint) { return rec.received(); }
			receiver rec;
			bytes savedData1;
			bytes savedData2;
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "sender");
		ABI_CHECK(callContractFunction("recv(uint256)", 7), vector<bytes>());
		ABI_CHECK(callContractFunction("val()"), encodeArgs(0));
		ABI_CHECK(callContractFunction("forward(bool)", true), encodeArgs(true));
		ABI_CHECK(callContractFunction("val()"), encodeArgs(8));
		ABI_CHECK(callContractFunction("forward(bool)", false), encodeArgs(true));
		ABI_CHECK(callContractFunction("val()"), encodeArgs(16));
		ABI_CHECK(callContractFunction("forward(bool)", true), encodeArgs(true));
		ABI_CHECK(callContractFunction("val()"), encodeArgs(0x80));
	}
}

BOOST_AUTO_TEST_CASE(copy_from_calldata_removes_bytes_data)
{
	char const* sourceCode = R"(
		contract c {
			function set(bytes memory b) public returns (bool) { data = b; return true; }
			function set2(bytes memory b) external { data = b; }
			bytes data;
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction("set(bytes)", encodeDyn(string("12345"))), encodeArgs(true));
		BOOST_CHECK(!storageEmpty(m_contractAddress));
		callContractFunction("set2(bytes)", encodeDyn(string("")));
		BOOST_CHECK(m_transactionSuccessful);
		BOOST_CHECK(m_output.empty());
		BOOST_CHECK(storageEmpty(m_contractAddress));
	);
}

BOOST_AUTO_TEST_CASE(storing_invalid_boolean)
{
	char const* sourceCode = R"(
		contract C {
			event Ev(bool);
			bool public perm;
			function set() public returns(uint) {
				bool tmp;
				assembly {
					tmp := 5
				}
				perm = tmp;
				return 1;
			}
			function ret() public returns(bool) {
				bool tmp;
				assembly {
					tmp := 5
				}
				return tmp;
			}
			function ev() public returns(uint) {
				bool tmp;
				assembly {
					tmp := 5
				}
				emit Ev(tmp);
				return 1;
			}
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("set()"), encodeArgs(1));
		ABI_CHECK(callContractFunction("perm()"), encodeArgs(1));
		ABI_CHECK(callContractFunction("ret()"), encodeArgs(1));
		ABI_CHECK(callContractFunction("ev()"), encodeArgs(1));
		BOOST_REQUIRE_EQUAL(numLogs(), 1);
		BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
		BOOST_CHECK(logData(0) == encodeLogs(1));
		BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
		BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Ev(bool)")));
	}
}

BOOST_AUTO_TEST_CASE(struct_referencing)
{
	static char const* sourceCode = R"(
		pragma abicoder v2;
		interface I {
			struct S { uint a; }
		}
		library L {
			struct S { uint b; uint a; }
			function f() public pure returns (S memory) {
				S memory s;
				s.a = 3;
				return s;
			}
			function g() public pure returns (I.S memory) {
				I.S memory s;
				s.a = 4;
				return s;
			}
			// argument-dependant lookup tests
			function a(I.S memory) public pure returns (uint) { return 1; }
			function a(S memory) public pure returns (uint) { return 2; }
		}
		contract C is I {
			function f() public pure returns (S memory) {
				S memory s;
				s.a = 1;
				return s;
			}
			function g() public pure returns (I.S memory) {
				I.S memory s;
				s.a = 2;
				return s;
			}
			function h() public pure returns (L.S memory) {
				L.S memory s;
				s.a = 5;
				return s;
			}
			function x() public pure returns (L.S memory) {
				return L.f();
			}
			function y() public pure returns (I.S memory) {
				return L.g();
			}
			function a1() public pure returns (uint) { S memory s; return L.a(s); }
			function a2() public pure returns (uint) { L.S memory s; return L.a(s); }
		}
	)";
	compileAndRun(sourceCode, 0, "L");
	ABI_CHECK(callContractFunction("f()"), encodeArgs(encodeRefArgs(bigint(0), bigint(3))));
	ABI_CHECK(callContractFunction("g()"), encodeArgs(encodeRefArgs(bigint(4))));
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{ {"L", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(encodeRefArgs(bigint(1))));
	ABI_CHECK(callContractFunction("g()"), encodeArgs(encodeRefArgs(bigint(2))));
	ABI_CHECK(callContractFunction("h()"), encodeArgs(encodeRefArgs(bigint(0), bigint(5))));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(encodeRefArgs(bigint(0), bigint(3))));
	ABI_CHECK(callContractFunction("y()"), encodeArgs(encodeRefArgs(bigint(4))));
	ABI_CHECK(callContractFunction("a1()"), encodeArgs(1));
	ABI_CHECK(callContractFunction("a2()"), encodeArgs(2));
}

BOOST_AUTO_TEST_CASE(enum_referencing)
{
	char const* sourceCode = R"(
		interface I {
			enum Direction { A, B, Left, Right }
		}
		library L {
			enum Direction { Left, Right }
			function f() public pure returns (Direction) {
				return Direction.Right;
			}
			function g() public pure returns (I.Direction) {
				return I.Direction.Right;
			}
		}
		contract C is I {
			function f() public pure returns (Direction) {
				return Direction.Right;
			}
			function g() public pure returns (I.Direction) {
				return I.Direction.Right;
			}
			function h() public pure returns (L.Direction) {
				return L.Direction.Right;
			}
			function x() public pure returns (L.Direction) {
				return L.f();
			}
			function y() public pure returns (I.Direction) {
				return L.g();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "L");
	ABI_CHECK(callContractFunction("f()"), encodeArgs(1));
	ABI_CHECK(callContractFunction("g()"), encodeArgs(3));
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"L", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(3));
	ABI_CHECK(callContractFunction("g()"), encodeArgs(3));
	ABI_CHECK(callContractFunction("h()"), encodeArgs(1));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(1));
	ABI_CHECK(callContractFunction("y()"), encodeArgs(3));
}

//TODO: this.call
/*
BOOST_AUTO_TEST_CASE(bytes_in_arguments)
{
	char const* sourceCode = R"(
		contract c {
			uint result;
			function f(uint a, uint b) public { result += a + b; }
			function g(uint a) public { result *= a; }
			function test(uint a, bytes calldata data1, bytes calldata data2, uint b) external returns (uint r_a, uint r, uint r_b, uint l) {
				r_a = a;
				address(this).call(data1);
				address(this).call(data2);
				r = result;
				r_b = b;
				l = data1.length;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);

		string innercalldata1 = asString(FixedHash<4>(util::keccak256("f(uint,uint)")).asBytes() + encodeArgs(8, 9));
		string innercalldata2 = asString(FixedHash<4>(util::keccak256("g(uint)")).asBytes() + encodeArgs(3));
		bytes calldata = encodeArgs(
			12, 32 * 4, u256(32 * 4 + 32 + (innercalldata1.length() + 31) / 32 * 32), 13,
			u256(innercalldata1.length()), innercalldata1,
			u256(innercalldata2.length()), innercalldata2);
		ABI_CHECK(
			callContractFunction("test(uint,bytes,bytes,uint)", calldata),
			encodeArgs(12, (8 + 9) * 3, 13, u256(innercalldata1.length()))
		);
	);
}
*/

BOOST_AUTO_TEST_CASE(nested_dynamic_array_in_storage)
{
  char const* sourceCode = R"(
    contract A {
      uint[][] a;
      uint b;
      uint c;
      constructor() {
        c = 3;
        a = new uint[][](10);
        for (uint i = 0; i < 10; ++i)
          a[i] = new uint[](100);
      }
      function getLengths() public returns (uint l1, uint l2) { l1 = a.length; l2 = a[3].length; }
      function set(uint i, uint j, uint v, uint cv) public  { a[i][j] = v; c = cv; }
      function get(uint i, uint j) public returns (uint) { return a[i][j] + c; }
    }
  )";
  compileAndRun(sourceCode);
  ABI_CHECK(callContractFunction("getLengths()"), encodeArgs(10, 100));
  ABI_CHECK(callContractFunction("get(uint,uint)", 0, 0), encodeArgs(3));
  ABI_CHECK(callContractFunction("get(uint,uint)", 1, 0), encodeArgs(3));
  ABI_CHECK(callContractFunction("get(uint,uint)", 5, 54), encodeArgs(3));
  ABI_CHECK(callContractFunction("get(uint,uint)", 10, 0), vector<bytes>());
  ABI_CHECK(callContractFunction("get(uint,uint)", 0, 100), vector<bytes>());
  ABI_CHECK(callContractFunction("set(uint,uint,uint,uint)", 0, 100, 1, 30), vector<bytes>());
  ABI_CHECK(callContractFunction("get(uint,uint)", 0, 0), encodeArgs(3));
  ABI_CHECK(callContractFunction("get(uint,uint)", 1, 0), encodeArgs(3));
  ABI_CHECK(callContractFunction("get(uint,uint)", 5, 54), encodeArgs(3));
  ABI_CHECK(callContractFunction("set(uint,uint,uint,uint)", 1, 0, 2, 40), vector<bytes>());
  ABI_CHECK(callContractFunction("get(uint,uint)", 0, 0), encodeArgs(40));
  ABI_CHECK(callContractFunction("get(uint,uint)", 1, 0), encodeArgs(42));
  ABI_CHECK(callContractFunction("get(uint,uint)", 5, 54), encodeArgs(40));
  ABI_CHECK(callContractFunction("set(uint,uint,uint,uint)", 5, 54, 3, 50), vector<bytes>());
  ABI_CHECK(callContractFunction("get(uint,uint)", 0, 0), encodeArgs(50));
  ABI_CHECK(callContractFunction("get(uint,uint)", 1, 0), encodeArgs(52));
  ABI_CHECK(callContractFunction("get(uint,uint)", 5, 54), encodeArgs(53));
}

BOOST_AUTO_TEST_CASE(array_copy_storage_abi)
{
	// NOTE: This does not really test copying from storage to ABI directly,
	// because it will always copy to memory first.
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract c {
			uint8[] x;
			uint16[] y;
			uint24[] z;
			uint24[][] w;
			function test1() public returns (uint8[] memory) {
				for (uint i = 0; i < 101; ++i)
					x.push(uint8(i));
				return x;
			}
			function test2() public returns (uint16[] memory) {
				for (uint i = 0; i < 101; ++i)
					y.push(uint16(i));
				return y;
			}
			function test3() public returns (uint24[] memory) {
				for (uint i = 0; i < 101; ++i)
					z.push(uint24(i));
				return z;
			}
			function test4() public returns (uint24[][] memory) {
				w = new uint24[][](5);
				for (uint i = 0; i < 5; ++i)
					for (uint j = 0; j < 101; ++j)
						w[i].push(uint24(j));
				return w;
			}
		}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode);
		bytes valueSequence;
		for (ssize_t i = 100; i >= 0; --i)
			valueSequence += toBigEndian(u256(i));
        bytes valueSequence2;
        for (auto b : valueSequence) {
                if (b != 100) {
                        valueSequence2.push_back(0);
                }
                valueSequence2.push_back(b);
        }
        bytes valueSequence3;
        bytes valueSequence4;
        for (auto b : valueSequence) {
                if (b != 100) {
                        valueSequence3.push_back(0);
                        valueSequence3.push_back(0);
                }
                valueSequence4.push_back(0);
                valueSequence4.push_back(0);
                valueSequence3.push_back(b);
                valueSequence4.push_back(b);
        }
        ABI_CHECK(callContractFunction("test1()"), vector<bytes>(1, valueSequence + encodeRefArgs(0x01, 101)));
        ABI_CHECK(callContractFunction("test2()"), vector<bytes>(1, valueSequence2 + encodeRefArgs(0x01, 101)));
        ABI_CHECK(callContractFunction("test3()"), vector<bytes>(1, valueSequence3 + encodeRefArgs(0x01, 101)));
        ABI_CHECK(callContractFunction("test4()"), vector<bytes>(1,
                valueSequence3 + encodeRefArgs(0x01, 101) +
                valueSequence4 + encodeRefArgs(0x01, 101) +
                valueSequence4 + encodeRefArgs(0x01, 101) +
                valueSequence4 + encodeRefArgs(0x01, 101) +
                valueSequence4 + encodeRefArgs(0x01, 101) +
                 encodeRefArgs(0x01, 5)
        ));
	);
}

//BOOST_AUTO_TEST_CASE(assignment_to_const_array_vars)
//{
//	char const* sourceCode = R"(
//		contract C {
//			uint[3] constant x = [uint(1), 2, 3];
//			uint constant y = x[0] + x[1] + x[2];
//			function f() public returns (uint) { return y; }
//		}
//	)";
//	compileAndRun(sourceCode);
//	ABI_CHECK(callContractFunction("f()"), encodeArgs(1 + 2 + 3));
//}

// Disabled until https://github.com/ethereum/solidity/issues/715 is implemented
//BOOST_AUTO_TEST_CASE(constant_struct)
//{
//	char const* sourceCode = R"(
//		contract C {
//			struct S { uint x; uint[] y; }
//			S constant x = S(5, new uint[](4));
//			function f() public returns (uint) { return x.x; }
//		}
//	)";
//	compileAndRun(sourceCode);
//	ABI_CHECK(callContractFunction("f()"), encodeArgs(5));
//}

BOOST_AUTO_TEST_CASE(packed_storage_structs_delete)
{
	char const* sourceCode = R"(
		contract C {
			struct str { uint8 a; uint16 b; uint8 c; }
			uint8 x;
			uint16 y;
			str data;
			function test() public returns (uint) {
				x = 1;
				y = 2;
				data.a = 2;
				data.b = 0xabcd;
				data.c = 0xfa;
				if (x != 1 || y != 2 || data.a != 2 || data.b != 0xabcd || data.c != 0xfa)
					return 2;
				delete y;
				delete data.b;
				if (x != 1 || y != 0 || data.a != 2 || data.b != 0 || data.c != 0xfa)
					return 3;
				delete x;
				delete data;
				return 1;
			}
		}
	)";
	compileAndRun(sourceCode);
	ABI_CHECK(callContractFunction("test()"), encodeArgs(1));
	BOOST_CHECK(storageEmpty(m_contractAddress));
}

BOOST_AUTO_TEST_CASE(invalid_enum_logged)
{
	char const* sourceCode = R"(
		contract C {
			enum X { A, B }
			event Log(X);

			function test_log_ok() public returns (uint) {
				X x = X.A;
				emit Log(x);
				return 1;
			}
		}
		)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("test_log_ok()"), encodeArgs(u256(1)));
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 1);
	BOOST_REQUIRE_EQUAL(logTopic(0, 0), util::keccak256(string("Log(uint8)")));
	BOOST_CHECK_EQUAL(h256(logData(0)), h256(u256(0)));
}

BOOST_AUTO_TEST_CASE(evm_exceptions_in_constructor_out_of_baund)
{
	char const* sourceCode = R"(
		contract A {
			uint public test = 1;
			uint[3] arr;
			constructor()
			{
				uint index = 5;
				test = arr[index];
				++test;
			}
		}
	)";
	compileAndRunWithoutCheck({{"", sourceCode}}, 0, "A");
	BOOST_CHECK(m_status != 0);
}

BOOST_AUTO_TEST_CASE(failing_send)
{
	char const* sourceCode = R"(
		contract Helper {
			uint[] data;
			fallback () external {
				data[9]; // trigger exception
			}
		}
		contract Main {
			constructor() payable {}
			function callHelper(address payable _a) public returns (bool r, uint bal) {
				r = !_a.send(5);
				bal = address(this).balance;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Helper");
	h160 const c_helperAddress = m_contractAddress;
	compileAndRun(sourceCode, 20, "Main");
	BOOST_REQUIRE(callContractFunction("callHelper(address)", c_helperAddress) == encodeArgs(true, 20));
}

BOOST_AUTO_TEST_CASE(return_string)
{
	char const* sourceCode = R"(
		contract Main {
			string public s;
			function set(string calldata _s) external {
				s = _s;
			}
			function get1() public returns (string memory r) {
				return s;
			}
			function get2() public returns (string memory r) {
				r = s;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Main");
	string s("Julia");
	vector<bytes> args = vector<bytes>(1, encodeRefArgs(uint64_t(s.length()), s));
	BOOST_REQUIRE(callContractFunctionNoEncoding("set(string)", args) == encodeArgs());
	ABI_CHECK(callContractFunction("get1()"), args);
	ABI_CHECK(callContractFunction("get2()"), args);
	ABI_CHECK(callContractFunction("s()"), args);
}

BOOST_AUTO_TEST_CASE(return_multiple_strings_of_various_sizes)
{
	char const* sourceCode = R"(
		contract Main {
			string public s1;
			string public s2;
			function set(string calldata _s1, uint x, string calldata _s2) external returns (uint) {
				s1 = _s1;
				s2 = _s2;
				return x;
			}
			function get() public returns (string memory r1, string memory r2) {
				r1 = s1;
				r2 = s2;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Main");
	string s1(
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
	);
	string s2(
		"ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ"
		"ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ"
		"ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ"
		"ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ"
		"ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ"
	);
	vector<size_t> lengths{0, 30, 32, 63, 64, 65, 210, 300};
	for (auto l1: lengths)
		for (auto l2: lengths)
		{
			bytes dyn1 = encodeDyn(s1.substr(0, l1));
			bytes dyn2 = encodeDyn(s2.substr(0, l2));
			vector<bytes> args = encodeArgs(dyn1, u256(l1), dyn2);
			BOOST_REQUIRE(
				callContractFunctionNoEncoding("set(string,uint,string)",args) ==
				encodeArgs(u256(l1))
			);
			vector<bytes> result = encodeArgs(dyn1, dyn2);
			ABI_CHECK(callContractFunction("get()"), result);
			ABI_CHECK(callContractFunction("s1()"), vector<bytes>(1, dyn1));
			ABI_CHECK(callContractFunction("s2()"), vector<bytes>(1, dyn2));
		}
}

BOOST_AUTO_TEST_CASE(accessor_involving_strings)
{
	char const* sourceCode = R"(
		contract Main {
			struct stringData { string a; uint b; string c; }
			mapping(uint => stringData[]) public data;
			function set(uint x, uint y, string calldata a, uint b, string calldata c) external returns (bool) {
				while (data[x].length < y + 1)
					data[x].push();
				data[x][y].a = a;
				data[x][y].b = b;
				data[x][y].c = c;
				return true;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Main");
	string s1("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
	string s2("ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ");
	bytes s1Data = encodeDyn(s1);
	bytes s2Data = encodeDyn(s2);
	u256 b = 765;
	u256 x = 7;
	u256 y = 123;
	vector<bytes> args = encodeArgs(x, y, s1Data, b, s2Data);
	vector<bytes> result = encodeArgs(s1Data, b, s2Data);
	BOOST_REQUIRE(callContractFunctionNoEncoding("set(uint,uint,string,uint,string)", args) == encodeArgs(true));
	BOOST_REQUIRE(callContractFunction("data(uint,uint)", x, y) == result);
}

BOOST_AUTO_TEST_CASE(bytes_in_function_calls)
{
	char const* sourceCode = R"(
		contract Main {
			string public s1;
			string public s2;
			function set(string memory _s1, uint x, string memory _s2) public returns (uint) {
				s1 = _s1;
				s2 = _s2;
				return x;
			}
			function setIndirectFromMemory(string memory _s1, uint x, string memory _s2) public returns (uint) {
				return this.set(_s1, x, _s2);
			}
			function setIndirectFromCalldata(string calldata _s1, uint x, string calldata _s2) external returns (uint) {
				return this.set(_s1, x, _s2);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Main");
	string s1("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
	string s2("ABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZABCDEFGHIJKLMNOPQRSTUVXYZ");
	vector<size_t> lengths{0, 31, 64, 65};
	for (auto l1: lengths)
		for (auto l2: lengths)
		{
			bytes dyn1 = encodeDyn(s1.substr(0, l1));
			bytes dyn2 = encodeDyn(s2.substr(0, l2));
			vector<bytes> args1 = encodeArgs(dyn1, u256(l1), dyn2);
			BOOST_REQUIRE(
				callContractFunctionNoEncoding("setIndirectFromMemory(string,uint,string)", args1) ==
				encodeArgs(u256(l1))
			);
			ABI_CHECK(callContractFunction("s1()"), encodeArgs(dyn1));
			ABI_CHECK(callContractFunction("s2()"), encodeArgs(dyn2));
			// swapped
			vector<bytes> args2 = encodeArgs(dyn2, u256(l1), dyn1);
			BOOST_REQUIRE(
				callContractFunctionNoEncoding("setIndirectFromCalldata(string,uint,string)", args2) ==
				encodeArgs(u256(l1))
			);
			ABI_CHECK(callContractFunction("s1()"), encodeArgs(dyn2));
			ABI_CHECK(callContractFunction("s2()"), encodeArgs(dyn1));
		}
}

BOOST_AUTO_TEST_CASE(return_bytes_internal)
{
	char const* sourceCode = R"(
		contract Main {
			bytes s1;
			function doSet(bytes memory _s1) public returns (bytes memory _r1) {
				s1 = _s1;
				_r1 = s1;
			}
			function set(bytes calldata _s1) external returns (uint _r, bytes memory _r1) {
				_r1 = doSet(_s1);
				_r = _r1.length;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Main");
	string s1("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
	vector<size_t> lengths{31, 64, 65};
	bytes dyn1 = encodeRefArgs(0);
	vector<bytes> args1 = encodeArgs(dyn1);
	ABI_CHECK(
		callContractFunction("set(bytes)", args1),
		encodeArgs(u256(0), dyn1)
	);
	for (auto l1: lengths)
	{
		bytes dyn1 = encodeRefArgs(uint64_t(l1), s1.substr(0, l1));
		vector<bytes> args1 = encodeArgs(dyn1);
		ABI_CHECK(
			callContractFunction("set(bytes)", args1),
			encodeArgs(u256(l1), dyn1)
		);
	}
}

BOOST_AUTO_TEST_CASE(memory_types_initialisation)
{
	char const* sourceCode = R"(
		contract Test {
			mapping(uint=>uint) data;
			function stat() public returns (uint[5] memory)
			{
				data[2] = 3; // make sure to use some memory
			}
			function dyn() public returns (uint[] memory) { stat(); }
			function nested() public returns (uint[3][] memory) { stat(); }
			function nestedStat() public returns (uint[3][7] memory) { stat(); }
		}
	)";
	compileAndRun(sourceCode, 0, "Test");

	ABI_CHECK(callContractFunction("stat()"), vector<bytes>(1, encodeRefArray(vector<u256>(5), 0)));
	ABI_CHECK(callContractFunction("dyn()"), vector<bytes>(1, encodeRefArgs(1)));
	ABI_CHECK(callContractFunction("nested()"), vector<bytes>(1, encodeRefArgs(1)));
	ABI_CHECK(callContractFunction("nestedStat()"), vector<bytes>(1, encodeRefArray(vector<u256>(3 * 7), 0)));
}

BOOST_AUTO_TEST_CASE(calldata_struct_short)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			struct S { uint256 a; uint256 b; }
			function f(S calldata s) external pure returns (uint) {
				return abi.encode(s).length;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");

	// double check that the valid case goes through
	ABI_CHECK(callContractFunctionNoEncoding("f((uint256,uint256))", encodeArgs(encodeRefArgs(u256(1), u256(2)))), encodeArgs(0x40));

	ABI_CHECK(callContractFunctionNoEncoding("f((uint256,uint256))", vector<bytes>(1, bytes(63,0))), encodeArgs(0x40));
	ABI_CHECK(callContractFunctionNoEncoding("f((uint256,uint256))", vector<bytes>(1, bytes(33,0))), encodeArgs(0x40));
	ABI_CHECK(callContractFunctionNoEncoding("f((uint256,uint256))", vector<bytes>(1, bytes(32,0))), encodeArgs(0x40));
	ABI_CHECK(callContractFunctionNoEncoding("f((uint256,uint256))", vector<bytes>(1, bytes(31,0))), encodeArgs(0x40));
	ABI_CHECK(callContractFunctionNoEncoding("f((uint256,uint256))", vector<bytes>(1, bytes())), encodeArgs(0x40));
}

BOOST_AUTO_TEST_CASE(calldata_struct_function_type)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			struct S { function (uint) external returns (uint) fn; }
			function f(S calldata s) external returns (uint) {
				return s.fn(42);
			}
			function g(uint a) external returns (uint) {
				return a * 3;
			}
			function h(uint a) external returns (uint) {
				return 23;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");

	u160 contractAddress = m_contractAddress;
	ABI_CHECK(callContractFunctionNoEncoding("f((function))", encodeArgs(encodeRefArgs(contractAddress, int16_t(2)))), encodeArgs(42 * 3));
	ABI_CHECK(callContractFunctionNoEncoding("f((function))", encodeArgs(encodeRefArgs(contractAddress, int16_t(3)))), encodeArgs(23));
}

BOOST_AUTO_TEST_CASE(calldata_array_dynamic_three_dimensional)
{
	vector<vector<vector<u256>>> data {
		{
			{ 0x010A01, 0x010A02, 0x010A03 },
			{ 0x010B01, 0x010B02, 0x010B03 }
		},
		{
			{ 0x020A01, 0x020A02, 0x020A03 },
			{ 0x020B01, 0x020B02, 0x020B03 }
		}
	};

	for (bool outerDynamicallySized: { true, false })
	for (bool middleDynamicallySized: { true, false })
	for (bool innerDynamicallySized: { true, false })
	{
		// only test dynamically encoded arrays
		if (!outerDynamicallySized && !middleDynamicallySized && !innerDynamicallySized)
			continue;

		string arrayType = "uint";
		arrayType += innerDynamicallySized ? "[]" : "[3]";
		arrayType += middleDynamicallySized ? "[]" : "[2]";
		arrayType += outerDynamicallySized ? "[]" : "[2]";

		string sourceCode = R"(
			pragma abicoder v2;
			contract C {
				function test()" + arrayType + R"( calldata a) external returns (uint) {
					return a.length;
				}
				function test()" + arrayType + R"( calldata a, uint i) external returns (uint) {
					return a[i].length;
				}
				function test()" + arrayType + R"( calldata a, uint i, uint j) external returns (uint) {
					return a[i][j].length;
				}
				function test()" + arrayType + R"( calldata a, uint i, uint j, uint k) external returns (uint) {
					return a[i][j][k];
				}
				function reenc()" + arrayType + R"( calldata a, uint i, uint j, uint k) external returns (uint) {
					return this.test(a, i, j, k);
				}
			}
		)";
		compileAndRun(sourceCode, 0, "C");

		bytes encoding;
		for (int i = data.size() - 1; i >= 0; --i) {
			for (int j = data[i].size() - 1; j >= 0; --j) {
				for (int k = data[i][j].size() - 1; k >= 0; --k) {
					encoding += encodeLog(bigint(data[i][j][k]));
				}
				if (innerDynamicallySized)
					encoding += encodeRefArgs(1, int(data[i][j].size()));
			}
			if (middleDynamicallySized)
				encoding += encodeRefArgs(1, int(data[i].size()));
		}
		if (outerDynamicallySized)
			encoding += encodeRefArgs(1, int(data.size()));
		while (encoding.size() >= 2 && encoding[0] == 0 && encoding[1] <= 0x7f) {
			encoding.erase(encoding.begin());
		}
		while (encoding.size() >= 2 && encoding[0] == 0xff && encoding[1] >= 0x80) {
			encoding.erase(encoding.begin());
		}

		ABI_CHECK(callContractFunction("test(" + arrayType + ")", encoding), encodeArgs(data.size()));
		for (size_t i = 0; i < data.size(); i++)
		{
			ABI_CHECK(callContractFunction("test(" + arrayType + ",uint)", encoding, i), encodeArgs(data[i].size()));
			for (size_t j = 0; j < data[i].size(); j++)
			{
				ABI_CHECK(callContractFunction("test(" + arrayType + ",uint,uint)", encoding, i, j), encodeArgs(data[i][j].size()));
				for (size_t k = 0; k < data[i][j].size(); k++)
				{
					ABI_CHECK(callContractFunction("test(" + arrayType + ",uint,uint,uint)", encoding, i, j, k), encodeArgs(data[i][j][k]));
					ABI_CHECK(callContractFunction("reenc(" + arrayType + ",uint,uint,uint)", encoding, i, j, k), encodeArgs(data[i][j][k]));
				}
				callContractFunction("test(" + arrayType + ",uint,uint,uint)", encoding, i, j, data[i][j].size());
				BOOST_CHECK_EQUAL(m_status, 255);
			}
			callContractFunction("test(" + arrayType + ",uint,uint)", encoding, i, data[i].size());
			BOOST_CHECK_EQUAL(m_status, 255);
		}
		callContractFunction("test(" + arrayType + ",uint)", encoding, data.size());
		BOOST_CHECK_EQUAL(m_status, 255);
	}
}

BOOST_AUTO_TEST_CASE(literal_strings)
{
	char const* sourceCode = R"(
		contract Test {
			string public long;
			string public medium;
			string public short;
			string public empty;
			function f() public returns (string memory) {
				long = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
				medium = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
				short = "123";
				empty = "";
				return "Hello, World!";
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Test");
	string longStr = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
	string medium = "01234567890123456789012345678901234567890123456789012345678901234567890123456789";
	string shortStr = "123";
	string hello = "Hello, World!";

	ABI_CHECK(callContractFunction("f()"), vector<bytes>(1, encodeDyn(hello)));
	ABI_CHECK(callContractFunction("long()"), vector<bytes>(1, encodeDyn(longStr)));
	ABI_CHECK(callContractFunction("medium()"), vector<bytes>(1, encodeDyn(medium)));
	ABI_CHECK(callContractFunction("short()"), vector<bytes>(1, encodeDyn(shortStr)));
	ABI_CHECK(callContractFunction("empty()"), vector<bytes>(1, encodeDyn(string())));
}

BOOST_AUTO_TEST_CASE(initialise_string_constant)
{
	char const* sourceCode = R"(
		contract Test {
			string public short = "abcdef";
			string public long = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
		}
	)";
	compileAndRun(sourceCode, 0, "Test");
	string longStr = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
	string shortStr = "abcdef";


	ABI_CHECK(callContractFunction("long()"), vector<bytes>(1, encodeDyn(longStr)));
	ABI_CHECK(callContractFunction("short()"), vector<bytes>(1, encodeDyn(shortStr)));
}

BOOST_AUTO_TEST_CASE(string_as_mapping_key)
{
	char const* sourceCode = R"(
		contract Test {
			mapping(string => uint) data;
			function set(string memory _s, uint _v) public { data[_s] = _v; }
			function get(string memory _s) public returns (uint) { return data[_s]; }
		}
	)";

	vector<string> strings{
		"Hello, World!",
		"Hello,                            World!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1111",
		"",
		"1"
	};

	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode, 0, "Test");
		for (unsigned i = 0; i < strings.size(); i++)
			ABI_CHECK(callContractFunction(
				"set(string,uint)",
				encodeDyn(strings[i]),
				u256(7 + i)
			), encodeArgs());
		for (unsigned i = 0; i < strings.size(); i++)
			ABI_CHECK(callContractFunction(
				"get(string)",
				encodeDyn(strings[i])
			), encodeArgs(u256(7 + i)));
	)
}

BOOST_AUTO_TEST_CASE(string_as_public_mapping_key)
{
	char const* sourceCode = R"(
		contract Test {
			mapping(string => uint) public data;
			function set(string memory _s, uint _v) public { data[_s] = _v; }
		}
	)";
	compileAndRun(sourceCode, 0, "Test");
	vector<string> strings{
		"Hello, World!",
		"Hello,                            World!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1111",
		"",
		"1"
	};
	for (unsigned i = 0; i < strings.size(); i++)
		ABI_CHECK(callContractFunction(
			"set(string,uint)",
			encodeDyn(strings[i]),
			int(7 + i)
		), encodeArgs());
	for (unsigned i = 0; i < strings.size(); i++)
		ABI_CHECK(callContractFunction(
			"data(string)",
			encodeDyn(strings[i])
		), encodeArgs(int(7 + i)));
}

BOOST_AUTO_TEST_CASE(nested_string_as_public_mapping_key)
{
	char const* sourceCode = R"(
		contract Test {
			mapping(string => mapping(string => uint)) public data;
			function set(string memory _s, string memory _s2, uint _v) public {
				data[_s][_s2] = _v; }
		}
	)";
	compileAndRun(sourceCode, 0, "Test");
	vector<string> strings{
		"Hello, World!",
		"Hello,                            World!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1111",
		"",
		"1",
		"last one"
	};
	for (unsigned i = 0; i + 1 < strings.size(); i++)
		ABI_CHECK(callContractFunction(
			"set(string,string,uint)",
			encodeDyn(strings[i]),
			encodeDyn(strings[i+1]),
			int(7 + i)
		), encodeArgs());
	for (unsigned i = 0; i + 1 < strings.size(); i++)
		ABI_CHECK(callContractFunction(
			"data(string,string)",
			encodeDyn(strings[i]),
			encodeDyn(strings[i+1])
		), encodeArgs(int(7 + i)));
}

BOOST_AUTO_TEST_CASE(nested_mixed_string_as_public_mapping_key)
{
	char const* sourceCode = R"(
		contract Test {
			mapping(string =>
				mapping(int =>
					mapping(address =>
						mapping(bytes => int)))) public data;

			function set(
				string memory _s1,
				int _s2,
				address _s3,
				bytes memory _s4,
				int _value
			) public
			{
				data[_s1][_s2][_s3][_s4] = _value;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Test");

	struct Index
	{
		string s1;
		int s2;
		int s3;
		string s4;
	};

	vector<Index> data{
		{ "aabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabcbc", 4, 23, "efg" },
		{ "tiaron", 456, 63245, "908apzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapzapz" },
		{ "", 2345, 12934, "665i65i65i65i65i65i65i65i65i65i65i65i65i65i65i65i65i65i5iart" },
		{ "¡¿…", 9781, 8148, "" },
		{ "ρν♀♀ω₂₃♀", 929608, 303030, "" }
	};

	for (size_t i = 0; i + 1 < data.size(); i++)
		ABI_CHECK(callContractFunction(
			"set(string,int,address,bytes,int)",
			encodeDyn(data[i].s1),
			data[i].s2,
			data[i].s3,
			encodeDyn(data[i].s4),
			i - 3
		), encodeArgs());
	for (size_t i = 0; i + 1 < data.size(); i++)
		ABI_CHECK(callContractFunction(
			"data(string,int,address,bytes)",
			encodeDyn(data[i].s1),
			data[i].s2,
			data[i].s3,
			encodeDyn(data[i].s4)
		), encodeArgs(i - 3));
}

BOOST_AUTO_TEST_CASE(constant_string_literal)
{
	char const* sourceCode = R"(
		contract Test {
			bytes32 constant public b = "abcdefghijklmnopq";
			string constant public x = "abefghijklmnopqabcdefghijklmnopqabcdefghijklmnopqabca";

			constructor() {
				string memory xx = x;
				bytes32 bb = b;
			}
			function getB() public returns (bytes32) { return b; }
			function getX() public returns (string memory) { return x; }
			function getX2() public returns (string memory r) { r = x; }
			function unused() public returns (uint) {
				"unusedunusedunusedunusedunusedunusedunusedunusedunusedunusedunusedunused";
				return 2;
			}
		}
	)";

	compileAndRun(sourceCode);
	string longStr = "abefghijklmnopqabcdefghijklmnopqabcdefghijklmnopqabca";
	string shortStr = "abcdefghijklmnopq" + string(15, 0);
	ABI_CHECK(callContractFunction("b()"), encodeArgs(shortStr));
	ABI_CHECK(callContractFunction("x()"), vector<bytes>(1, encodeDyn(longStr)));
	ABI_CHECK(callContractFunction("getB()"), encodeArgs(shortStr));
	ABI_CHECK(callContractFunction("getX()"), vector<bytes>(1, encodeDyn(longStr)));
	ABI_CHECK(callContractFunction("getX2()"), vector<bytes>(1, encodeDyn(longStr)));
	ABI_CHECK(callContractFunction("unused()"), encodeArgs(2));
}

BOOST_AUTO_TEST_CASE(library_call)
{
	char const* sourceCode = R"(
		library Lib { function m(uint x, uint y) public returns (uint) { return x * y; } }
		contract Test {
			function f(uint x) public returns (uint) {
				return Lib.m(x, 9);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(33)), encodeArgs(u256(33) * 9));
}

BOOST_AUTO_TEST_CASE(library_function_external)
{
	char const* sourceCode = R"(
		library Lib { function m(bytes calldata b) external pure returns (bytes1) { return b[2]; } }
		contract Test {
			function f(bytes memory b) public pure returns (bytes1) {
				return Lib.m(b);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(bytes)", encodeArgs(encodeDyn(string("abcde")))), encodeArgs("c"));
}

BOOST_AUTO_TEST_CASE(library_stray_values)
{
	char const* sourceCode = R"(
		library Lib { function m(uint x, uint y) public returns (uint) { return x * y; } }
		contract Test {
			function f(uint x) public returns (uint) {
				Lib;
				Lib.m;
				return x + 9;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(33)), encodeArgs(u256(42)));
}

BOOST_AUTO_TEST_CASE(internal_types_in_library)
{
	char const* sourceCode = R"(
		library Lib {
			function find(uint16[] storage _haystack, uint16 _needle) public view returns (uint256)
			{
				for (uint256 i = 0; i < _haystack.length; ++i)
					if (_haystack[i] == _needle)
						return i;
				return type(uint256).max;
			}
		}
		contract Test {
			mapping(string => uint16[]) data;
			function f() public returns (uint256 a, uint256 b)
			{
				while (data["abc"].length < 20)
					data["abc"].push();
				data["abc"][4] = 9;
				data["abc"][17] = 3;
				a = Lib.find(data["abc"], 9);
				b = Lib.find(data["abc"], 3);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(4), u256(17)));
}

BOOST_AUTO_TEST_CASE(mapping_arguments_in_library)
{
	char const* sourceCode = R"(
		library Lib {
			function set(mapping(uint => uint) storage m, uint key, uint value) internal
			{
				m[key] = value;
			}
			function get(mapping(uint => uint) storage m, uint key) internal view returns (uint)
			{
				return m[key];
			}
		}
		contract Test {
			mapping(uint => uint) m;
			function set(uint256 key, uint256 value) public returns (uint)
			{
				uint oldValue = Lib.get(m, key);
				Lib.set(m, key, value);
				return oldValue;
			}
			function get(uint256 key) public view returns (uint) {
				return Lib.get(m, key);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("set(uint256,uint256)", u256(1), u256(42)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(uint256,uint256)", u256(2), u256(84)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(uint256,uint256)", u256(21), u256(7)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(1)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(2)), encodeArgs(u256(84)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(21)), encodeArgs(u256(7)));
	ABI_CHECK(callContractFunction("set(uint256,uint256)", u256(1), u256(21)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("set(uint256,uint256)", u256(2), u256(42)), encodeArgs(u256(84)));
	ABI_CHECK(callContractFunction("set(uint256,uint256)", u256(21), u256(14)), encodeArgs(u256(7)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(1)), encodeArgs(u256(21)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(2)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("get(uint256)", u256(21)), encodeArgs(u256(14)));
}

BOOST_AUTO_TEST_CASE(mapping_returns_in_library)
{
	char const* sourceCode = R"(
		library Lib {
			function choose_mapping(mapping(uint => uint) storage a, mapping(uint => uint) storage b, bool c) internal pure returns(mapping(uint=>uint) storage)
			{
				return c ? a : b;
			}
		}
		contract Test {
			mapping(uint => uint) a;
			mapping(uint => uint) b;
			function set(bool choice, uint256 key, uint256 value) public returns (uint)
			{
				mapping(uint => uint) storage m = Lib.choose_mapping(a, b, choice);
				uint oldValue = m[key];
				m[key] = value;
				return oldValue;
			}
			function get(bool choice, uint256 key) public view returns (uint) {
				return Lib.choose_mapping(a, b, choice)[key];
			}
			function get_a(uint256 key) public view returns (uint) {
				return a[key];
			}
			function get_b(uint256 key) public view returns (uint) {
				return b[key];
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", true, u256(1), u256(42)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", true, u256(2), u256(84)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", true, u256(21), u256(7)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", false, u256(1), u256(10)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", false, u256(2), u256(11)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", false, u256(21), u256(12)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(1)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(2)), encodeArgs(u256(84)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(21)), encodeArgs(u256(7)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(1)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(2)), encodeArgs(u256(84)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(21)), encodeArgs(u256(7)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(1)), encodeArgs(u256(10)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(2)), encodeArgs(u256(11)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(21)), encodeArgs(u256(12)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(1)), encodeArgs(u256(10)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(2)), encodeArgs(u256(11)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(21)), encodeArgs(u256(12)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", true, u256(1), u256(21)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", true, u256(2), u256(42)), encodeArgs(u256(84)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", true, u256(21), u256(14)), encodeArgs(u256(7)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", false, u256(1), u256(30)), encodeArgs(u256(10)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", false, u256(2), u256(31)), encodeArgs(u256(11)));
	ABI_CHECK(callContractFunction("set(bool,uint256,uint256)", false, u256(21), u256(32)), encodeArgs(u256(12)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(1)), encodeArgs(u256(21)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(2)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("get_a(uint256)", u256(21)), encodeArgs(u256(14)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(1)), encodeArgs(u256(21)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(2)), encodeArgs(u256(42)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", true, u256(21)), encodeArgs(u256(14)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(1)), encodeArgs(u256(30)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(2)), encodeArgs(u256(31)));
	ABI_CHECK(callContractFunction("get_b(uint256)", u256(21)), encodeArgs(u256(32)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(0)), encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(1)), encodeArgs(u256(30)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(2)), encodeArgs(u256(31)));
	ABI_CHECK(callContractFunction("get(bool,uint256)", false, u256(21)), encodeArgs(u256(32)));
}

BOOST_AUTO_TEST_CASE(mapping_returns_in_library_named)
{
	char const* sourceCode = R"(
		library Lib {
			function f(mapping(uint => uint) storage a, mapping(uint => uint) storage b) internal returns(mapping(uint=>uint) storage r)
			{
				r = a;
				r[1] = 42;
				r = b;
				r[1] = 21;
			}
		}
		contract Test {
			mapping(uint => uint) a;
			mapping(uint => uint) b;
			function f() public returns (uint, uint, uint, uint, uint, uint)
			{
				Lib.f(a, b)[2] = 84;
				return (a[0], a[1], a[2], b[0], b[1], b[2]);
			}
			function g() public returns (uint, uint, uint, uint, uint, uint)
			{
				mapping(uint => uint) storage m = Lib.f(a, b);
				m[2] = 17;
				return (a[0], a[1], a[2], b[0], b[1], b[2]);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(0), u256(42), u256(0), u256(0), u256(21), u256(84)));
	ABI_CHECK(callContractFunction("g()"), encodeArgs(u256(0), u256(42), u256(0), u256(0), u256(21), u256(17)));
}

BOOST_AUTO_TEST_CASE(using_library_mappings_public)
{
	char const* sourceCode = R"(
			library Lib {
				function set(mapping(uint => uint) storage m, uint key, uint value) public
				{
					m[key] = value;
				}
			}
			contract Test {
				mapping(uint => uint) m1;
				mapping(uint => uint) m2;
				function f() public returns (uint, uint, uint, uint, uint, uint)
				{
					Lib.set(m1, 0, 1);
					Lib.set(m1, 2, 42);
					Lib.set(m2, 0, 23);
					Lib.set(m2, 2, 99);
					return (m1[0], m1[1], m1[2], m2[0], m2[1], m2[2]);
				}
			}
		)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(1), u256(0), u256(42), u256(23), u256(0), u256(99)));
}

BOOST_AUTO_TEST_CASE(using_library_mappings_external)
{
	char const* libSourceCode = R"(
			library Lib {
				function set(mapping(uint => uint) storage m, uint key, uint value) external
				{
					m[key] = value * 2;
				}
			}
		)";
	char const* sourceCode = R"(
			library Lib {
				function set(mapping(uint => uint) storage m, uint key, uint value) external
				{
					m[key] = value * 2;
				}
			}
			contract Test {
				mapping(uint => uint) m1;
				mapping(uint => uint) m2;
				function f() public returns (uint, uint, uint, uint, uint, uint)
				{
					Lib.set(m1, 0, 1);
					Lib.set(m1, 2, 42);
					Lib.set(m2, 0, 23);
					Lib.set(m2, 2, 99);
					return (m1[0], m1[1], m1[2], m2[0], m2[1], m2[2]);
				}
			}
		)";
	for (auto v2: {false, true})
	{
		string prefix = "pragma abicoder " + string(v2 ? "v2" : "v1") + ";\n";
		compileAndRun(prefix + libSourceCode, 0, "Lib");
		compileAndRun(prefix + sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
		ABI_CHECK(callContractFunction("f()"), encodeArgs(2, 0, 84, 46, 0, 198));
	}
}

BOOST_AUTO_TEST_CASE(using_library_mappings_return)
{
	char const* sourceCode = R"(
			library Lib {
				function choose(mapping(uint => mapping(uint => uint)) storage m, uint key) external returns (mapping(uint => uint) storage) {
					return m[key];
				}
			}
			contract Test {
				mapping(uint => mapping(uint => uint)) m;
				function f() public returns (uint, uint, uint, uint, uint, uint)
				{
					Lib.choose(m, 0)[0] = 1;
					Lib.choose(m, 0)[2] = 42;
					Lib.choose(m, 1)[0] = 23;
					Lib.choose(m, 1)[2] = 99;
					return (m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2]);
				}
			}
		)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(1), u256(0), u256(42), u256(23), u256(0), u256(99)));
}

BOOST_AUTO_TEST_CASE(using_library_structs)
{
	char const* sourceCode = R"(
		library Lib {
			struct Data { uint a; uint[] b; }
			function set(Data storage _s) public
			{
				_s.a = 7;
				while (_s.b.length < 20)
					_s.b.push();
				_s.b[19] = 8;
			}
		}
		contract Test {
			mapping(string => Lib.Data) data;
			function f() public returns (uint a, uint b)
			{
				Lib.set(data["abc"]);
				a = data["abc"].a;
				b = data["abc"].b[19];
			}
		}
	)";
	compileAndRun(sourceCode, 0, "Lib");
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"Lib", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(7), u256(8)));
}

BOOST_AUTO_TEST_CASE(short_strings)
{
	// This test verifies that the byte array encoding that combines length and data works
	// correctly.
	char const* sourceCode = R"(
		contract A {
			bytes public data1 = "123";
			bytes data2;
			function lengthChange() public returns (uint)
			{
				// store constant in short and long string
				data1 = "123";
				if (!equal(data1, "123")) return 1;
				data2 = "12345678901234567890123456789012345678901234567890a";
				if (data2[17] != "8") return 3;
				if (data2.length != 51) return 4;
				if (data2[data2.length - 1] != "a") return 5;
				// change length: short -> short
				while (data1.length < 5)
					data1.push();
				if (data1.length != 5) return 6;
				data1[4] = "4";
				if (data1[0] != "1") return 7;
				if (data1[4] != "4") return 8;
				// change length: short -> long
				while (data1.length < 80)
					data1.push();
				if (data1.length != 80) return 9;
				while (data1.length > 70)
					data1.pop();
				if (data1.length != 70) return 9;
				if (data1[0] != "1") return 10;
				if (data1[4] != "4") return 11;
				for (uint i = 0; i < data1.length; i ++)
					data1[i] = bytes1(uint8(i * 3));
				if (uint8(data1[4]) != 4 * 3) return 12;
				if (uint8(data1[67]) != 67 * 3) return 13;
				// change length: long -> short
				while (data1.length > 22)
					data1.pop();
				if (data1.length != 22) return 14;
				if (uint8(data1[21]) != 21 * 3) return 15;
				if (uint8(data1[2]) != 2 * 3) return 16;
				// change length: short -> shorter
				while (data1.length > 19)
					data1.pop();
				if (data1.length != 19) return 17;
				if (uint8(data1[7]) != 7 * 3) return 18;
				// and now again to original size
				while (data1.length < 22)
					data1.push();
				if (data1.length != 22) return 19;
				if (data1[21] != 0) return 20;
				while (data1.length > 0)
					data1.pop();
				while (data2.length > 0)
					data2.pop();
			}
			function copy() public returns (uint) {
				bytes memory x = "123";
				bytes memory y = "012345678901234567890123456789012345678901234567890123456789";
				bytes memory z = "1234567";
				data1 = x;
				data2 = y;
				if (!equal(data1, x)) return 1;
				if (!equal(data2, y)) return 2;
				// lengthen
				data1 = y;
				if (!equal(data1, y)) return 3;
				// shorten
				data1 = x;
				if (!equal(data1, x)) return 4;
				// change while keeping short
				data1 = z;
				if (!equal(data1, z)) return 5;
				// copy storage -> storage
				data1 = x;
				data2 = y;
				// lengthen
				data1 = data2;
				if (!equal(data1, y)) return 6;
				// shorten
				data1 = x;
				data2 = data1;
				if (!equal(data2, x)) return 7;
				bytes memory c = data2;
				data1 = c;
				if (!equal(data1, x)) return 8;
				data1 = "";
				data2 = "";
			}
			function deleteElements() public returns (uint) {
				data1 = "01234";
				delete data1[2];
				if (data1[2] != 0) return 1;
				if (data1[0] != "0") return 2;
				if (data1[3] != "3") return 3;
				delete data1;
				if (data1.length != 0) return 4;
			}

			function equal(bytes storage a, bytes memory b) internal returns (bool) {
				if (a.length != b.length) return false;
				for (uint i = 0; i < a.length; ++i) if (a[i] != b[i]) return false;
				return true;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "A");
	ABI_CHECK(callContractFunction("data1()"), vector<bytes>(1, encodeDyn(string("123"))));
	ABI_CHECK(callContractFunction("lengthChange()"), encodeArgs(u256(0)));
	BOOST_CHECK(storageEmpty(u160(m_contractAddress)));
	ABI_CHECK(callContractFunction("deleteElements()"), encodeArgs(u256(0)));
	BOOST_CHECK(storageEmpty(u160(m_contractAddress)));
	ABI_CHECK(callContractFunction("copy()"), encodeArgs(u256(0)));
	BOOST_CHECK(storageEmpty(m_contractAddress));
}

BOOST_AUTO_TEST_CASE(calldata_offset)
{
	// This tests a specific bug that was caused by not using the correct memory offset in the
	// calldata unpacker.
	char const* sourceCode = R"(
		contract CB
		{
			address[] _arr;
			string public last = "nd";
			constructor(address[] memory guardians)
			{
				_arr = guardians;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "CB", encodeArgs(encodeRefArray({u256(0x20), u256(0x00)}, 2, 20)));
	ABI_CHECK(callContractFunction("last()"), encodeArgs(encodeDyn(string("nd"))));
}

BOOST_AUTO_TEST_CASE(reject_ether_sent_to_library)
{
	char const* sourceCode = R"(
		library lib {}
		contract c {
			constructor() payable {}
			function f(address payable x) public returns (bool) {
				return x.send(1);
			}
			receive () external payable {}
		}
	)";
	compileAndRun(sourceCode, 0, "lib");
	Address libraryAddress = m_contractAddress;
	compileAndRun(sourceCode, 10, "c");
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 10);
	BOOST_CHECK_EQUAL(balanceAt(libraryAddress), 0);
	ABI_CHECK(callContractFunction("f(address)", encodeArgs(libraryAddress)), encodeArgs(false));
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 10);
	BOOST_CHECK_EQUAL(balanceAt(libraryAddress), 0);
	ABI_CHECK(callContractFunction("f(address)", encodeArgs(m_contractAddress)), encodeArgs(true));
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 10);
	BOOST_CHECK_EQUAL(balanceAt(libraryAddress), 0);
}

BOOST_AUTO_TEST_CASE(create_memory_array_allocation_size)
{
	// Check allocation size of byte array. Should be 32 plus length rounded up to next
	// multiple of 32
	char const* sourceCode = R"(
		contract C {
			function f() public pure returns (uint d1, uint d2, uint d3, uint memsize) {
				bytes memory b1 = new bytes(31);
				bytes memory b2 = new bytes(32);
				bytes memory b3 = new bytes(256);
				bytes memory b4 = new bytes(31);
				assembly {
					d1 := sub(b2, b1)
					d2 := sub(b3, b2)
					d3 := sub(b4, b3)
					memsize := msize()
				}
			}
		}
	)";
	if (!m_optimiserSettings.runYulOptimiser)
	{
		if (false) {
			compileAndRun(sourceCode, 0, "C");
			ABI_CHECK(callContractFunction("f()"), encodeArgs(0x40, 0x40, 0x20 + 256, 0x260));
		}
	}
}

BOOST_AUTO_TEST_CASE(using_for_function_on_int)
{
	char const* sourceCode = R"(
		library D { function double(uint self) public returns (uint) { return 2*self; } }
		contract C {
			using D for uint;
			function f(uint a) public returns (uint) {
				return a.double();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"D", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(9)), encodeArgs(u256(2 * 9)));
}

BOOST_AUTO_TEST_CASE(using_for_function_on_struct)
{
	char const* sourceCode = R"(
		library D { struct s { uint a; } function mul(s storage self, uint x) public returns (uint) { return self.a *= x; } }
		contract C {
			using D for D.s;
			D.s public x;
			function f(uint a) public returns (uint) {
				x.a = 3;
				return x.mul(a);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"D", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(7)), encodeArgs(u256(3 * 7)));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(3 * 7)));
}

BOOST_AUTO_TEST_CASE(using_for_overload)
{
	char const* sourceCode = R"(
		library D {
			struct s { uint a; }
			function mul(s storage self, uint x) public returns (uint) { return self.a *= x; }
			function mul(s storage self, bytes32 x) public returns (bytes32) { }
		}
		contract C {
			using D for D.s;
			D.s public x;
			function f(uint a) public returns (uint) {
				x.a = 6;
				return x.mul(a);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"D", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(7)), encodeArgs(u256(6 * 7)));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(6 * 7)));
}

BOOST_AUTO_TEST_CASE(using_for_by_name)
{
	char const* sourceCode = R"(
		library D { struct s { uint a; } function mul(s storage self, uint x) public returns (uint) { return self.a *= x; } }
		contract C {
			using D for D.s;
			D.s public x;
			function f(uint a) public returns (uint) {
				x.a = 6;
				return x.mul({x: a});
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"D", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(7)), encodeArgs(u256(6 * 7)));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(6 * 7)));
}

BOOST_AUTO_TEST_CASE(bound_function_in_function)
{
	char const* sourceCode = R"(
		library L {
			function g(function() internal returns (uint) _t) internal returns (uint) { return _t(); }
		}
		contract C {
			using L for *;
			function f() public returns (uint) {
				return t.g();
			}
			function t() public pure returns (uint)  { return 7; }
		}
	)";
	compileAndRun(sourceCode, 0, "L");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"L", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(7)));
}

BOOST_AUTO_TEST_CASE(bound_function_in_var)
{
	char const* sourceCode = R"(
		library D { struct s { uint a; } function mul(s storage self, uint x) public returns (uint) { return self.a *= x; } }
		contract C {
			using D for D.s;
			D.s public x;
			function f(uint a) public returns (uint) {
				x.a = 6;
				return (x.mul)({x: a});
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"D", m_contractAddress}});
	ABI_CHECK(callContractFunction("f(uint)", u256(7)), encodeArgs(u256(6 * 7)));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(6 * 7)));
}

BOOST_AUTO_TEST_CASE(bound_function_to_string)
{
	char const* sourceCode = R"(
		library D { function length(string memory self) public returns (uint) { return bytes(self).length; } }
		contract C {
			using D for string;
			string x;
			function f() public returns (uint) {
				x = "abc";
				return x.length();
			}
			function g() public returns (uint) {
				string memory s = "abc";
				return s.length();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"D", m_contractAddress}});
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(3)));
	ABI_CHECK(callContractFunction("g()"), encodeArgs(u256(3)));
}

BOOST_AUTO_TEST_CASE(inline_long_string_return)
{
		char const* sourceCode = R"(
		contract C {
			function f() public returns (string memory) {
				return (["somethingShort", "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"][1]);
			}
		}
	)";

	string strLong = "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678900123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789001234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("f()"), vector<bytes>(1, encodeDyn(strLong)));
}

BOOST_AUTO_TEST_CASE(index_access_with_type_conversion)
{
	// Test for a bug where higher order bits cleanup was not done for array index access.
	char const* sourceCode = R"(
			contract C {
				function f(uint x) public returns (uint[256] memory r){
					r[uint8(x)] = 2;
				}
			}
	)";
	compileAndRun(sourceCode, 0, "C");
	// neither of the two should throw due to out-of-bounds access
	BOOST_CHECK(callContractFunction("f(uint)", u256(0x01))[0].size() == 256 * 9 - 8);
	BOOST_CHECK(callContractFunction("f(uint)", u256(0x101))[0].size() == 256 * 9 - 8);
}

BOOST_AUTO_TEST_CASE(failed_create)
{
	char const* sourceCode = R"(
		contract D { constructor() payable {} }
		contract C {
			uint public x;
			constructor() payable {}
			function f(uint amount) public returns (D) {
				x++;
				return (new D){value: uint256(amount)}();
			}
			function stack(uint depth) public returns (address) {
				if (depth < 1024)
					return this.stack(depth - 1);
				else
					return address(f(0));
			}
		}
	)";
	compileAndRun(sourceCode, 20, "C");
	BOOST_CHECK(callContractFunction("f(uint)", 20) != encodeArgs(u256(0)));
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(1)));
	ABI_CHECK(callContractFunction("f(uint)", 20), encodeArgs());
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(1)));
	ABI_CHECK(callContractFunction("stack(uint)", 1023), encodeArgs());
	ABI_CHECK(callContractFunction("x()"), encodeArgs(u256(1)));
}

BOOST_AUTO_TEST_CASE(correctly_initialize_memory_array_in_constructor)
{
	// Memory arrays are initialized using calldatacopy past the size of the calldata.
	// This test checks that it also works in the constructor context.
	char const* sourceCode = R"(
		contract C {
			bool public success;
			constructor() {
				// Make memory dirty.
				assembly {
					for { let i := 0 } lt(i, 64) { i := add(i, 1) } {
						mstore(msize(), not(0))
					}
				}
				uint16[3] memory c;
				require(c[0] == 0 && c[1] == 0 && c[2] == 0);
				uint16[] memory x = new uint16[](3);
				require(x[0] == 0 && x[1] == 0 && x[2] == 0);
				success = true;
			}
		}
	)";
	// Cannot run against yul optimizer because of msize
	if (!m_optimiserSettings.runYulOptimiser)
	{
		if (false) {
			compileAndRun(sourceCode, 0, "C");
			ABI_CHECK(callContractFunction("success()"), encodeArgs(u256(1)));
		}
	}
}

BOOST_AUTO_TEST_CASE(mutex)
{
	char const* sourceCode = R"(
		contract mutexed {
			bool locked;
			modifier protected {
				if (locked) revert();
				locked = true;
				_;
				locked = false;
			}
		}
		abstract contract Payable {
			function deposit() external payable virtual;
		}
		contract Fund is mutexed {
			uint shares;
			constructor() payable { shares = msg.value; }
			function withdraw(uint amount) public protected returns (uint) {
				// NOTE: It is very bad practice to write this function this way.
				// Please refer to the documentation of how to do this properly.
				if (amount > shares) revert();
				Payable(msg.sender).deposit{value: uint256(amount)}();
				shares -= amount;
				return shares;
			}
			function withdrawUnprotected(uint amount) public returns (uint) {
				// NOTE: It is very bad practice to write this function this way.
				// Please refer to the documentation of how to do this properly.
				if (amount > shares) revert();
				Payable(msg.sender).deposit{value: uint256(amount)}();
				shares -= amount;
				return shares;
			}
		}
		contract Attacker is Payable {
			Fund public fund;
			uint callDepth;
			bool protected;
			function setProtected(bool _protected) public { protected = _protected; }
			constructor(Fund _fund) { fund = _fund; }
			function attack() public returns (uint) {
				callDepth = 0;
				return attackInternal();
			}
			function attackInternal() internal returns (uint) {
				if (protected)
					return fund.withdraw(10);
				else
					return fund.withdrawUnprotected(10);
			}
			function deposit() external payable override {
				callDepth++;
				if (callDepth < 4)
					attackInternal();
			}
		}
	)";
	compileAndRun(sourceCode, 500, "Fund");
	u160 const fund = m_contractAddress;
	BOOST_CHECK_EQUAL(balanceAt(fund), 500);
	compileAndRun(sourceCode, 0, "Attacker", encodeArgs(fund));
	ABI_CHECK(callContractFunction("setProtected(bool)", true), encodeArgs());
	ABI_CHECK(callContractFunction("attack()"), encodeArgs());
	BOOST_CHECK_EQUAL(balanceAt(fund), 500);
	ABI_CHECK(callContractFunction("setProtected(bool)", false), encodeArgs());
	ABI_CHECK(callContractFunction("attack()"), encodeArgs(u256(460)));
	BOOST_CHECK_EQUAL(balanceAt(fund), 460);
}

BOOST_AUTO_TEST_CASE(payable_function)
{
	char const* sourceCode = R"(
		contract C {
			uint public a;
			function f() payable public returns (uint) {
				return msg.value;
			}
			fallback() external payable {
				a = msg.value + 1;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunctionWithValue("f()", 27), encodeArgs(u256(27)));
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 27);
	ABI_CHECK(callFallbackWithValue(27), encodeArgs());
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 27 + 27);
	ABI_CHECK(callContractFunction("a()"), encodeArgs(u256(28)));
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 27 + 27);
}

BOOST_AUTO_TEST_CASE(payable_function_calls_library)
{
	char const* sourceCode = R"(
		library L {
			function f() public returns (uint) { return 7; }
		}
		contract C {
			function f() public payable returns (uint) {
				return L.f();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "L");
	compileAndRun(sourceCode, 0, "C", vector<bytes>(), map<string, h160>{{"L", m_contractAddress}});
	ABI_CHECK(callContractFunctionWithValue("f()", 27), encodeArgs(u256(7)));
}

BOOST_AUTO_TEST_CASE(non_payable_throw)
{
	char const* sourceCode = R"(
		contract C {
			uint public a;
			function f() public returns (uint) {
				return msgvalue();
			}
			function msgvalue() internal returns (uint) {
				return msg.value;
			}
			fallback() external {
				update();
			}
			function update() internal {
				a = msg.value + 1;
			}

		}
	)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunctionWithValue("f()", 27), encodeArgs());
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 0);
	ABI_CHECK(callFallback(), encodeArgs());
	ABI_CHECK(callContractFunction("a()"), encodeArgs(u256(1)));
	ABI_CHECK(callFallbackWithValue(27), encodeArgs());
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 0);
	ABI_CHECK(callContractFunction("a()"), encodeArgs(u256(1)));
	ABI_CHECK(callContractFunctionWithValue("a()", 27), encodeArgs());
	BOOST_CHECK_EQUAL(balanceAt(m_contractAddress), 0);
}

BOOST_AUTO_TEST_CASE(mem_resize_is_not_paid_at_call)
{
	// This tests that memory resize for return values is not paid during the call, which would
	// make the gas calculation overly complex. We access the end of the output area before
	// the call is made.
	// Tests that this also survives the optimizer.
	char const* sourceCode = R"(
		contract C {
			function f() public returns (uint[200] memory) {}
		}
		contract D {
			function f(C c) public returns (uint) { c.f(); return 7; }
		}
	)";

	compileAndRun(sourceCode, 0, "C");
	h160 const cAddr = m_contractAddress;
	compileAndRun(sourceCode, 0, "D");
	ABI_CHECK(callContractFunction("f(address)", cAddr), encodeArgs(u256(7)));
}

BOOST_AUTO_TEST_CASE(receive_external_function_type)
{
	char const* sourceCode = R"(
		contract C {
			function g() public returns (uint) { return 7; }
			function f(function() external returns (uint) g) public returns (uint) {
				return g();
			}
		}
	)";

	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction(
		"f(function)",
		m_contractAddress, 1
	), encodeArgs(u256(7)));
}

BOOST_AUTO_TEST_CASE(return_external_function_type)
{
	char const* sourceCode = R"(
		contract C {
			function g() public {}
			function f() public returns (function() external) {
				return this.g;
			}
		}
	)";

	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(
		callContractFunction("f()"),
		encodeArgs(u160(m_contractAddress), 1)
	);
}

// TODO: store bound internal library functions

BOOST_AUTO_TEST_CASE(shift_bytes)
{
	char const* sourceCode = R"(
		contract C {
			function left(bytes20 x, uint8 y) public returns (bytes20) {
				return x << y;
			}
			function right(bytes20 x, uint8 y) public returns (bytes20) {
				return x >> y;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("left(bytes20,uint8)", "12345678901234567890", 8 * 8), encodeArgs("901234567890" + string(8, 0)));
	ABI_CHECK(callContractFunction("right(bytes20,uint8)", "12345678901234567890", 8 * 8), encodeArgs("123456789012"));
}

BOOST_AUTO_TEST_CASE(shift_bytes_cleanup)
{
	char const* sourceCode = R"(
		contract C {
			function left(uint8 y) public returns (bytes20) {
				bytes25 x0 = "12345678901234567890abcde";
				bytes20 x = bytes20(x0);
				return x << y;
			}
			function right(uint8 y) public returns (bytes20) {
				bytes25 x0 = "12345678901234567890abcde";
				bytes20 x = bytes20(x0);
				return x >> y;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("left(uint8)", 8 * 8), encodeArgs("901234567890" + string(8, 0)));
	ABI_CHECK(callContractFunction("right(uint8)", 8 * 8), encodeArgs("123456789012"));
}

BOOST_AUTO_TEST_CASE(contracts_separated_with_comment)
{
	char const* sourceCode = R"(
		contract C1 {}
		/**
		**/
		contract C2 {}
	)";
	ALSO_VIA_YUL(
		DISABLE_EWASM_TESTRUN()

		compileAndRun(sourceCode, 0, "C1");
		compileAndRun(sourceCode, 0, "C2");
	)
}

BOOST_AUTO_TEST_CASE(include_creation_bytecode_only_once)
{
	char const* sourceCode = R"(
		contract D {
			bytes a = hex"1237651237125387136581271652831736512837126583171583712358126123765123712538713658127165283173651283712658317158371235812612376512371253871365812716528317365128371265831715837123581261237651237125387136581271652831736512837126583171583712358126";
			bytes b = hex"1237651237125327136581271252831736512837126583171383712358126123765125712538713658127165253173651283712658357158371235812612376512371a5387136581271652a317365128371265a317158371235812612a765123712538a13658127165a83173651283712a58317158371235a126";
			constructor(uint) {}
		}
		contract Double {
			function f() public {
				new D(2);
			}
			function g() public {
				new D(3);
			}
		}
		contract Single {
			function f() public {
				new D(2);
			}
		}
	)";
	compileAndRun(sourceCode);
	BOOST_CHECK_LE(
		double(m_compiler.object("Double").bytecode.size()),
		1.2 * double(m_compiler.object("Single").bytecode.size())
	);
}

BOOST_AUTO_TEST_CASE(recursive_struct_copy)
{
	char const* sourceCode = R"(
		contract C {
			struct S {
				S[] x;
			}
			S sstorage;
			function f() public returns (uint) {
				S memory s;
				s.x = new S[](10);
                s.x[5] = S(new S[](100));
				sstorage = s;
				return s.x[5].x.length;
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(100)));
}

BOOST_AUTO_TEST_CASE(revert_with_cause)
{
	char const* sourceCode = R"(
		contract D {
			string constant msg1 = "test1234567890123456789012345678901234567890";
			string msg2 = "test1234567890123456789012345678901234567890";
			function f() public {
				revert("test123");
			}
			function g() public {
				revert("test1234567890123456789012345678901234567890");
			}
			function h() public {
				revert(msg1);
			}
			function i() public {
				revert(msg2);
			}
			function j() public {
				string memory msg3 = "test1234567890123456789012345678901234567890";
				revert(msg3);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	ABI_CHECK(callContractFunction("f()"), encodeArgs());
        bigint expected = fromBigEndian<bigint>(encodeDyn(string("test123")));
        BOOST_CHECK_EQUAL(m_status, expected);
        expected = fromBigEndian<bigint>(encodeDyn(string("test1234567890123456789012345678901234567890")));
	ABI_CHECK(callContractFunction("g()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("h()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("i()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("j()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
}

BOOST_AUTO_TEST_CASE(require_with_message)
{
	char const* sourceCode = R"(
		contract D {
			bool flag = false;
			string storageError = "abc";
			string constant constantError = "abc";
			function f(uint x) public {
				require(x > 7, "failed");
			}
			function g() public {
				// As a side-effect of internalFun, the flag will be set to true
				// (even if the condition is true),
				// but it will only throw in the next evaluation.
				bool flagCopy = flag;
				require(flagCopy == false, internalFun());
			}
			function internalFun() public returns (string memory) {
				flag = true;
				return "only on second run";
			}
			function h() public {
				require(false, storageError);
			}
			function i() public {
				require(false, constantError);
			}
			function j() public {
				string memory errMsg = "msg";
				require(false, errMsg);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	ABI_CHECK(callContractFunction("f(uint)", 8), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, 0);
	ABI_CHECK(callContractFunction("f(uint)", 5), encodeArgs());
        bigint expected = fromBigEndian<bigint>(encodeDyn(string("failed")));
        BOOST_CHECK_EQUAL(m_status, expected);

	ABI_CHECK(callContractFunction("g()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, 0);
	ABI_CHECK(callContractFunction("g()"), encodeArgs());
        expected = fromBigEndian<bigint>(encodeDyn(string("only on second run")));
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("h()"), encodeArgs());
        expected = fromBigEndian<bigint>(encodeDyn(string("abc")));
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("i()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("j()"), encodeArgs());
        expected = fromBigEndian<bigint>(encodeDyn(string("msg")));
        BOOST_CHECK_EQUAL(m_status, expected);
}

BOOST_AUTO_TEST_CASE(bubble_up_error_messages)
{
	char const* sourceCode = R"(
		contract D {
			function f() public {
				revert("message");
			}
			function g() public {
				this.f();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	ABI_CHECK(callContractFunction("f()"), encodeArgs());
        bigint expected = fromBigEndian<bigint>(encodeDyn(string("message")));
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("g()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
}

BOOST_AUTO_TEST_CASE(bubble_up_error_messages_through_transfer)
{
	char const* sourceCode = R"(
		contract D {
			receive() external payable {
				revert("message");
			}
			function f() public {
				payable(this).transfer(0);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	ABI_CHECK(callFallback(), encodeArgs());
        bigint expected = fromBigEndian<bigint>(encodeDyn(string("message")));
        BOOST_CHECK_EQUAL(m_status, expected);
	ABI_CHECK(callContractFunction("f()"), encodeArgs());
        BOOST_CHECK_EQUAL(m_status, expected);
}

BOOST_AUTO_TEST_CASE(bubble_up_error_messages_through_create)
{
	char const* sourceCode = R"(
		contract E {
			constructor() {
				revert("message");
			}
		}
		contract D {
			function f() public {
				E x = new E();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "D");
	ABI_CHECK(callContractFunction("f()"), encodeArgs());
        bigint expected = fromBigEndian<bigint>(encodeDyn(string("message")));
        BOOST_CHECK_EQUAL(m_status, expected);
}

BOOST_AUTO_TEST_CASE(interface_contract)
{
	char const* sourceCode = R"(
		interface I {
			event A();
			function f() external returns (bool);
			fallback() external payable;
		}

		contract A is I {
			function f() public override returns (bool) {
				return g();
			}

			function g() public returns (bool) {
				return true;
			}

			fallback() override external payable {
			}
		}

		contract C {
			function f(address payable _interfaceAddress) public returns (bool) {
				I i = I(_interfaceAddress);
				return i.f();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "A");
	h160 const recipient = m_contractAddress;
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("f(address)", recipient), encodeArgs(true));
}

BOOST_AUTO_TEST_CASE(bare_call_invalid_address)
{
	char const* sourceCode = R"YY(
		contract C {
			/// Calling into non-existent account is successful (creates the account)
			function f() external returns (bool) {
				(bool success,) = address(0x4242).call("");
				return success;
			}
			function h() external returns (bool) {
				(bool success,) = address(0x4242).delegatecall("");
				return success;
			}
		}
	)YY";
	if (false) {
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(1)));
		ABI_CHECK(callContractFunction("g()"), encodeArgs(u256(1)));
		ABI_CHECK(callContractFunction("h()"), encodeArgs(u256(1)));
	}

	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
	{
		char const* sourceCode = R"YY(
			contract C {
				function f() external returns (bool, bytes memory) {
					return address(0x4242).staticcall("");
				}
			}
		)YY";
		if (false) {
			compileAndRun(sourceCode, 0, "C");
			ABI_CHECK(callContractFunction("f()"), encodeArgs(u256(1), 0x40, 0x00));
		}
	}
}

BOOST_AUTO_TEST_CASE(bare_call_return_data)
{
	if (solidity::test::CommonOptions::get().evmVersion().supportsReturndata())
	{
		vector<string> calltypes = {"call", "delegatecall"};
		if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
			calltypes.emplace_back("staticcall");
		for (string const& calltype: calltypes)
		{
			string sourceCode = R"DELIMITER(
				contract A {
					constructor() {
					}
					function return_bool() public pure returns(bool) {
						return true;
					}
					function return_int32() public pure returns(int32) {
						return -32;
					}
					function return_uint32() public pure returns(uint32) {
						return 0x3232;
					}
					function return_int256() public pure returns(int256) {
						return -256;
					}
					function return_uint256() public pure returns(uint256) {
						return 0x256256;
					}
					function return_bytes4() public pure returns(bytes4) {
						return 0xabcd0012;
					}
					function return_multi() public pure returns(bool, uint32, bytes4) {
						return (false, 0x3232, 0xabcd0012);
					}
					function return_bytes() public pure returns(bytes memory b) {
						b = new bytes(2);
						b[0] = 0x42;
						b[1] = 0x21;
					}
				}
				contract C {
					A addr;
					constructor() {
						addr = new A();
					}
					function f(string memory signature) public returns (bool, bytes memory) {
						return address(addr).)DELIMITER" + calltype + R"DELIMITER((abi.encodeWithSignature(signature));
					}
					function check_bool() external returns (bool) {
						(bool success, bytes memory data) = f("return_bool()");
						assert(success);
						bool a = abi.decode(data, (bool));
						assert(a);
						return true;
					}
					function check_int32() external returns (bool) {
						(bool success, bytes memory data) = f("return_int32()");
						assert(success);
						int32 a = abi.decode(data, (int32));
						assert(a == -32);
						return true;
					}
					function check_uint32() external returns (bool) {
						(bool success, bytes memory data) = f("return_uint32()");
						assert(success);
						uint32 a = abi.decode(data, (uint32));
						assert(a == 0x3232);
						return true;
					}
					function check_int256() external returns (bool) {
						(bool success, bytes memory data) = f("return_int256()");
						assert(success);
						int256 a = abi.decode(data, (int256));
						assert(a == -256);
						return true;
					}
					function check_uint256() external returns (bool) {
						(bool success, bytes memory data) = f("return_uint256()");
						assert(success);
						uint256 a = abi.decode(data, (uint256));
						assert(a == 0x256256);
						return true;
					}
					function check_bytes4() external returns (bool) {
						(bool success, bytes memory data) = f("return_bytes4()");
						assert(success);
						bytes4 a = abi.decode(data, (bytes4));
						assert(a == 0xabcd0012);
						return true;
					}
					function check_multi() external returns (bool) {
						(bool success, bytes memory data) = f("return_multi()");
						assert(success);
						(bool a, uint32 b, bytes4 c) = abi.decode(data, (bool, uint32, bytes4));
						assert(a == false && b == 0x3232 && c == 0xabcd0012);
						return true;
					}
					function check_bytes() external returns (bool) {
						(bool success, bytes memory data) = f("return_bytes()");
						assert(success);
						(bytes memory d) = abi.decode(data, (bytes));
						assert(d.length == 2 && d[0] == 0x42 && d[1] == 0x21);
						return true;
					}
				}
			)DELIMITER";
			if (false) {
				compileAndRun(sourceCode, 0, "C");
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_bool()"))), encodeArgs(true, 0x40, 0x20, true));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_int32()"))), encodeArgs(true, 0x40, 0x20, u256(-32)));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_uint32()"))), encodeArgs(true, 0x40, 0x20, u256(0x3232)));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_int256()"))), encodeArgs(true, 0x40, 0x20, u256(-256)));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_uint256()"))), encodeArgs(true, 0x40, 0x20, u256(0x256256)));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_bytes4()"))), encodeArgs(true, 0x40, 0x20, u256(0xabcd0012) << (28*8)));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_multi()"))), encodeArgs(true, 0x40, 0x60, false, u256(0x3232), u256(0xabcd0012) << (28*8)));
				ABI_CHECK(callContractFunction("f(string)", encodeDyn(string("return_bytes()"))), encodeArgs(true, 0x40, 0x60, 0x20, 0x02, encode(bytes{0x42,0x21}, false)));
				ABI_CHECK(callContractFunction("check_bool()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_int32()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_uint32()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_int256()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_uint256()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_bytes4()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_multi()"), encodeArgs(true));
				ABI_CHECK(callContractFunction("check_bytes()"), encodeArgs(true));
			}
		}
	}
}

BOOST_AUTO_TEST_CASE(abi_encodePacked)
{
	char const* sourceCode = R"(
		contract C {
			function f0() public pure returns (bytes memory) {
				return abi.encodePacked();
			}
			function f1() public pure returns (bytes memory) {
				return abi.encodePacked(uint8(1), uint8(2));
			}
			function f2() public pure returns (bytes memory) {
				string memory x = "abc";
				return abi.encodePacked(uint8(1), x, uint8(2));
			}
			function f3() public pure returns (bytes memory r) {
				// test that memory is properly allocated
				string memory x = "abc";
				r = abi.encodePacked(uint8(1), x, uint8(2));
				bytes memory y = "def";
				require(y[0] == "d");
				y[0] = "e";
				require(y[0] == "e");
			}
			function f4() public pure returns (bytes memory) {
				string memory x = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
				return abi.encodePacked(uint16(0x0701), x, uint16(0x1201));
			}
			function f_literal() public pure returns (bytes memory) {
				return abi.encodePacked(uint8(0x01), "abc", uint8(0x02));
			}
			function f_calldata (bytes memory data) public pure returns (bytes memory) {
				return abi.encodePacked(uint8(0x01), data, uint8(0x02));
			}
		}
	)";
	for (auto v2: {false, true})
	{
		string prefix = "pragma abicoder " + string(v2 ? "v2" : "v1") + ";\n";
		compileAndRun(prefix + sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f0()"), encodeArgs(encodeDyn(string(""))));
		ABI_CHECK(callContractFunction("f1()"), encodeArgs(encodeDyn(string("\x01\x02"))));
		ABI_CHECK(callContractFunction("f2()"), encodeArgs(encodeDyn(string("\x01" "abc" "\x02"))));
		ABI_CHECK(callContractFunction("f3()"), encodeArgs(encodeDyn(string("\x01" "abc" "\x02"))));
		ABI_CHECK(callContractFunction("f4()"), encodeArgs(encodeDyn(string("\x07\x01" "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" "\x12\x01"))));
		ABI_CHECK(callContractFunction("f_literal()"), encodeArgs(encodeDyn(string("\x01" "abc" "\x02"))));
		bytes data = FixedHash<4>(util::keccak256("f_calldata(bytes)")).asBytes();
		string dataString;
		for (uint8_t c : data) dataString.push_back((unsigned char) c);
		ABI_CHECK(callContractFunction("f_calldata(bytes)", encodeRefArgs(uint64_t(4), data)), encodeArgs(encodeDyn(string("\x01" + dataString + "\x02"))));
	}
}

BOOST_AUTO_TEST_CASE(abi_encodePacked_from_storage)
{
	char const* sourceCode = R"(
		contract C {
			uint24[9] small_fixed;
			int24[9] small_fixed_signed;
			uint24[] small_dyn;
			uint248[5] large_fixed;
			uint248[] large_dyn;
			bytes bytes_storage;
			function sf() public returns (bytes memory) {
				small_fixed[0] = 0xfffff1;
				small_fixed[2] = 0xfffff2;
				small_fixed[5] = 0xfffff3;
				small_fixed[8] = 0xfffff4;
				return abi.encodePacked(uint8(0x01), small_fixed, uint8(0x02));
			}
			function sd() public returns (bytes memory) {
				small_dyn.push(0xfffff1);
				small_dyn.push(0x00);
				small_dyn.push(0xfffff2);
				small_dyn.push(0x00);
				small_dyn.push(0x00);
				small_dyn.push(0xfffff3);
				small_dyn.push(0x00);
				small_dyn.push(0x00);
				small_dyn.push(0xfffff4);
				return abi.encodePacked(uint8(0x01), small_dyn, uint8(0x02));
			}
			function sfs() public returns (bytes memory) {
				small_fixed_signed[0] = -2;
				small_fixed_signed[2] = 0xffff2;
				small_fixed_signed[5] = -200;
				small_fixed_signed[8] = 0xffff4;
				return abi.encodePacked(uint8(0x01), small_fixed_signed, uint8(0x02));
			}
			function lf() public returns (bytes memory) {
				large_fixed[0] = 2**248-1;
				large_fixed[1] = 0xfffff2;
				large_fixed[2] = 2**248-2;
				large_fixed[4] = 0xfffff4;
				return abi.encodePacked(uint8(0x01), large_fixed, uint8(0x02));
			}
			function ld() public returns (bytes memory) {
				large_dyn.push(2**248-1);
				large_dyn.push(0xfffff2);
				large_dyn.push(2**248-2);
				large_dyn.push(0);
				large_dyn.push(0xfffff4);
				return abi.encodePacked(uint8(0x01), large_dyn, uint8(0x02));
			}
			function bytes_short() public returns (bytes memory) {
				bytes_storage = "abcd";
				return abi.encodePacked(uint8(0x01), bytes_storage, uint8(0x02));
			}
			function bytes_long() public returns (bytes memory) {
				bytes_storage = "0123456789012345678901234567890123456789";
				return abi.encodePacked(uint8(0x01), bytes_storage, uint8(0x02));
			}
		}
	)";
	for (auto v2: {false, true})
	{
		string prefix = "pragma abicoder " + string(v2 ? "v2" : "v1") + ";\n";
		compileAndRun(prefix + sourceCode, 0, "C");
        bytes payload = util::toBigEndian(u24(0xfffff1u)) +
                        util::toBigEndian(u24(0)) +
                        util::toBigEndian(u24(0xfffff2u)) +
                        util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                        util::toBigEndian(u24(0xfffff3u)) +
                        util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                        util::toBigEndian(u24(0xfffff4u));
		vector<bytes> encoded = encodeArgs(encodeDyn("\x01" + asString(payload) + "\x02"));
		ABI_CHECK(callContractFunction("sf()"), encoded);
		ABI_CHECK(callContractFunction("sd()"), encoded);
        payload = util::toBigEndian(u24(-2)) +
                  util::toBigEndian(u24(0)) +
                  util::toBigEndian(u24(0xffff2u)) +
                  util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                  util::toBigEndian(u24(-200)) +
                  util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                  util::toBigEndian(u24(0xffff4u));
		encoded = encodeArgs(encodeDyn("\x01" + asString(payload) + "\x02"));
		ABI_CHECK(callContractFunction("sfs()"), encoded);
		payload =
          util::toBigEndian(u248("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")) +
          util::toBigEndian(u248(0xfffff2u)) +
          util::toBigEndian(u248("0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe")) +
          util::toBigEndian(u248(0)) +
          util::toBigEndian(u248(0xfffff4u));
		encoded = encodeArgs(encodeDyn("\x01" + asString(payload) + "\x02"));
		ABI_CHECK(callContractFunction("lf()"), encoded);
		ABI_CHECK(callContractFunction("ld()"), encoded);
		ABI_CHECK(callContractFunction("bytes_short()"), encodeArgs(encodeDyn(string("\x01" "abcd\x02"))));
		ABI_CHECK(callContractFunction("bytes_long()"), encodeArgs(encodeDyn(string("\x01" "0123456789012345678901234567890123456789\x02"))));
	}
}

BOOST_AUTO_TEST_CASE(abi_encodePacked_from_memory)
{
	char const* sourceCode = R"(
		contract C {
			function sf() public pure returns (bytes memory) {
				uint24[9] memory small_fixed;
				small_fixed[0] = 0xfffff1;
				small_fixed[2] = 0xfffff2;
				small_fixed[5] = 0xfffff3;
				small_fixed[8] = 0xfffff4;
				return abi.encodePacked(uint8(0x01), small_fixed, uint8(0x02));
			}
			function sd() public pure returns (bytes memory) {
				uint24[] memory small_dyn = new uint24[](9);
				small_dyn[0] = 0xfffff1;
				small_dyn[2] = 0xfffff2;
				small_dyn[5] = 0xfffff3;
				small_dyn[8] = 0xfffff4;
				return abi.encodePacked(uint8(0x01), small_dyn, uint8(0x02));
			}
			function sfs() public pure returns (bytes memory) {
				int24[9] memory small_fixed_signed;
				small_fixed_signed[0] = -2;
				small_fixed_signed[2] = 0xffff2;
				small_fixed_signed[5] = -200;
				small_fixed_signed[8] = 0xffff4;
				return abi.encodePacked(uint8(0x01), small_fixed_signed, uint8(0x02));
			}
			function lf() public pure returns (bytes memory) {
				uint248[5] memory large_fixed;
				large_fixed[0] = 2**248-1;
				large_fixed[1] = 0xfffff2;
				large_fixed[2] = 2**248-2;
				large_fixed[4] = 0xfffff4;
				return abi.encodePacked(uint8(0x01), large_fixed, uint8(0x02));
			}
			function ld() public pure returns (bytes memory) {
				uint248[] memory large_dyn = new uint248[](5);
				large_dyn[0] = 2**248-1;
				large_dyn[1] = 0xfffff2;
				large_dyn[2] = 2**248-2;
				large_dyn[4] = 0xfffff4;
				return abi.encodePacked(uint8(0x01), large_dyn, uint8(0x02));
			}
		}
	)";
	for (auto v2: {false, true})
	{
		string prefix = "pragma abicoder " + string(v2 ? "v2" : "v1") + ";\n";
		compileAndRun(prefix + sourceCode, 0, "C");
        bytes payload = util::toBigEndian(u24(0xfffff1u)) +
                        util::toBigEndian(u24(0)) +
                        util::toBigEndian(u24(0xfffff2u)) +
                        util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                        util::toBigEndian(u24(0xfffff3u)) +
                        util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                        util::toBigEndian(u24(0xfffff4u));
		vector<bytes> encoded = encodeArgs(encodeDyn("\x01" + asString(payload) + "\x02"));
		ABI_CHECK(callContractFunction("sf()"), encoded);
		ABI_CHECK(callContractFunction("sd()"), encoded);
        payload = util::toBigEndian(u24(-2)) +
                  util::toBigEndian(u24(0)) +
                  util::toBigEndian(u24(0xffff2u)) +
                  util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                  util::toBigEndian(u24(-200)) +
                  util::toBigEndian(u24(0)) + util::toBigEndian(u24(0)) +
                  util::toBigEndian(u24(0xffff4u));
		encoded = encodeArgs(encodeDyn("\x01" + asString(payload) + "\x02"));
		ABI_CHECK(callContractFunction("sfs()"), encoded);
		payload =
          util::toBigEndian(u248("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")) +
          util::toBigEndian(u248(0xfffff2u)) +
          util::toBigEndian(u248("0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe")) +
          util::toBigEndian(u248(0)) +
          util::toBigEndian(u248(0xfffff4u));
		encoded = encodeArgs(encodeDyn("\x01" + asString(payload) + "\x02"));
		ABI_CHECK(callContractFunction("lf()"), encoded);
		ABI_CHECK(callContractFunction("ld()"), encoded);
	}
}

BOOST_AUTO_TEST_CASE(abi_encodePacked_functionPtr)
{
	char const* sourceCode = R"(
		contract C {
			C other = C(0x1112131400000000000011121314000000000087);
			function testDirect() public view returns (bytes memory) {
				return abi.encodePacked(uint8(8), other.f, uint8(2));
			}
			function testFixedArray() public view returns (bytes memory) {
				function () external pure returns (bytes memory)[1] memory x;
				x[0] = other.f;
				return abi.encodePacked(uint8(8), x, uint8(2));
			}
			function testDynamicArray() public view returns (bytes memory) {
				function () external pure returns (bytes memory)[] memory x = new function() external pure returns (bytes memory)[](1);
				x[0] = other.f;
				return abi.encodePacked(uint8(8), x, uint8(2));
			}
			function f() public pure returns (bytes memory) {}
		}
	)";
	for (auto v2: {false, true})
	{
		string prefix = "pragma abicoder " + string(v2 ? "v2" : "v1") + ";\n";
		compileAndRun(prefix + sourceCode, 0, "C");
        string directEncoding = asString(bytes{uint8_t(8)} +
                                         util::toBigEndian(u160("0x1112131400000000000011121314000000000087")) +
                                         util::toBigEndian(int16_t(0)) +
                                         bytes{uint8_t(2)});
		ABI_CHECK(callContractFunction("testDirect()"), encodeArgs(encodeDyn(directEncoding)));
		string arrayEncoding = directEncoding;
		ABI_CHECK(callContractFunction("testFixedArray()"), encodeArgs(encodeDyn(arrayEncoding)));
		ABI_CHECK(callContractFunction("testDynamicArray()"), encodeArgs(encodeDyn(arrayEncoding)));
	}
}

BOOST_AUTO_TEST_CASE(abi_encodePackedV2_structs)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			struct S {
				uint8 a;
				int16 b;
				uint8[2] c;
				int16[] d;
			}
			S s;
			event E(S indexed);
			constructor() {
				s.a = 0x12;
				s.b = -7;
				s.c[0] = 2;
				s.c[1] = 3;
				s.d.push(-7);
				s.d.push(-8);
			}
			function testStorage() public {
				s.a = 0x12;
				s.b = -7;
				s.c[0] = 2;
				s.c[1] = 3;
				s.d[0] = -7;
				s.d[1] = -8;
				emit E(s);
			}
			function testMemory() public {
				s.a = 0x12;
				s.b = -7;
				s.c[0] = 2;
				s.c[1] = 3;
				s.d[0] = -7;
				s.d[1] = -8;
				S memory m = s;
				emit E(m);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	bytes structEnc = bytes{uint8_t(0x12)} +
                      util::toBigEndian(int16_t(-7)) +
                      bytes{uint8_t(2), uint8_t(3)} +
                      util::toBigEndian(int16_t(-7)) +
                      util::toBigEndian(int16_t(-8));
	ABI_CHECK(callContractFunction("testStorage()"), encodeArgs());
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 2);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E((uint8,int16,uint8[2],int16[]))")));
	BOOST_CHECK_EQUAL(logTopic(0, 1), util::keccak256(structEnc));
	ABI_CHECK(callContractFunction("testMemory()"), encodeArgs());
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 2);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E((uint8,int16,uint8[2],int16[]))")));
	BOOST_CHECK_EQUAL(logTopic(0, 1), util::keccak256(structEnc));
}

BOOST_AUTO_TEST_CASE(abi_encodePackedV2_nestedArray)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			struct S {
				uint8 a;
				int16 b;
			}
			event E(S[2][][3] indexed);
			function testNestedArrays() public {
				S[2][][3] memory x;
				x[1] = new S[2][](2);
				x[1][0][0].a = 1;
				x[1][0][0].b = 2;
				x[1][0][1].a = 3;
				x[1][1][1].b = 4;
				emit E(x);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	bytes structEnc = bytes{uint8_t(1)} + util::toBigEndian(int16_t(2)) +
                      bytes{uint8_t(3)} + util::toBigEndian(int16_t(0)) +
                      bytes{uint8_t(0)} + util::toBigEndian(int16_t(0)) +
                      bytes{uint8_t(0)} + util::toBigEndian(int16_t(4));
	ABI_CHECK(callContractFunction("testNestedArrays()"), encodeArgs());
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 2);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E((uint8,int16)[2][][3])")));
	BOOST_CHECK_EQUAL(logTopic(0, 1), util::keccak256(structEnc));
}

BOOST_AUTO_TEST_CASE(abi_encodePackedV2_arrayOfStrings)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			string[] x;
			event E(string[] indexed);
			constructor() {
				x.push("abc");
				x.push("0123456789012345678901234567890123456789");
			}
			function testStorage() public {
				emit E(x);
			}
			function testMemory() public {
				string[] memory y = x;
				emit E(y);
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	bytes arrayEncoding = encode(string("abc")) +
                          encode(string("0123456789012345678901234567890123456789"));
	ABI_CHECK(callContractFunction("testStorage()"), encodeArgs());
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 2);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(string[])")));
	BOOST_CHECK_EQUAL(logTopic(0, 1), util::keccak256(arrayEncoding));
	ABI_CHECK(callContractFunction("testMemory()"), encodeArgs());
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 2);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E(string[])")));
	BOOST_CHECK_EQUAL(logTopic(0, 1), util::keccak256(arrayEncoding));
}

BOOST_AUTO_TEST_CASE(event_signature_in_library)
{
	// This tests a bug that was present where the "internal signature"
	// for structs was also used for events.
	char const* sourceCode = R"(
		pragma abicoder v2;
		library L {
			struct S {
				uint8 a;
				int16 b;
			}
			event E(S indexed, S);
			function f() internal {
				S memory s;
				emit E(s, s);
			}
		}
		contract C {
			constructor() {
				L.f();
			}
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 2);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("E((uint8,int16),(uint8,int16))")));
}

BOOST_AUTO_TEST_CASE(abi_encode_with_selector)
{
	char const* sourceCode = R"(
		contract C {
			function f0() public pure returns (bytes memory) {
				return abi.encodeWithSelector(0x12345678);
			}
			function f1() public pure returns (bytes memory) {
				return abi.encodeWithSelector(0x12345678, "abc");
			}
			function f2() public pure returns (bytes memory) {
				bytes4 x = 0x12345678;
				return abi.encodeWithSelector(x, "abc");
			}
			function f3() public pure returns (bytes memory) {
				bytes4 x = 0x12345678;
				return abi.encodeWithSelector(x, type(uint).max);
			}
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f0()"), encodeArgs(0x20, 4, "\x12\x34\x56\x78"));
		vector<bytes> expectation;
		expectation = encodeArgs(0x20, 4 + 0x60) + vector<bytes>{{0x12, 0x34, 0x56, 0x78}} + encodeArgs(0x20, 3, "abc") + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f1()"), expectation);
		expectation = encodeArgs(0x20, 4 + 0x60) + vector<bytes>{{0x12, 0x34, 0x56, 0x78}} + encodeArgs(0x20, 3, "abc") + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f2()"), expectation);
		expectation = encodeArgs(0x20, 4 + 0x20) + vector<bytes>{{0x12, 0x34, 0x56, 0x78}} + encodeArgs(u256(-1)) + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f3()"), expectation);
	}
}

BOOST_AUTO_TEST_CASE(abi_encode_with_selectorv2)
{
	char const* sourceCode = R"(
		pragma abicoder v2;
		contract C {
			function f0() public pure returns (bytes memory) {
				return abi.encodeWithSelector(0x12345678);
			}
			function f1() public pure returns (bytes memory) {
				return abi.encodeWithSelector(0x12345678, "abc");
			}
			function f2() public pure returns (bytes memory) {
				bytes4 x = 0x12345678;
				return abi.encodeWithSelector(x, "abc");
			}
			function f3() public pure returns (bytes memory) {
				bytes4 x = 0x12345678;
				return abi.encodeWithSelector(x, type(uint).max);
			}
			struct S { uint a; string b; uint16 c; }
			function f4() public pure returns (bytes memory) {
				bytes4 x = 0x12345678;
				S memory s;
				s.a = 0x1234567;
				s.b = "Lorem ipsum dolor sit ethereum........";
				s.c = 0x1234;
				return abi.encodeWithSelector(x, type(uint).max, s, uint(3));
			}
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f0()"), encodeArgs(0x20, 4, "\x12\x34\x56\x78"));
		vector<bytes> expectation;
		expectation = encodeArgs(0x20, 4 + 0x60) + vector<bytes>{{0x12, 0x34, 0x56, 0x78}} + encodeArgs(0x20, 3, "abc") + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f1()"), expectation);
		expectation = encodeArgs(0x20, 4 + 0x60) + vector<bytes>{{0x12, 0x34, 0x56, 0x78}} + encodeArgs(0x20, 3, "abc") + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f2()"), expectation);
		expectation = encodeArgs(0x20, 4 + 0x20) + vector<bytes>{{0x12, 0x34, 0x56, 0x78}} + encodeArgs(u256(-1)) + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f3()"), expectation);
		expectation =
			encodeArgs(0x20, 4 + 0x120) +
			vector<bytes>{{0x12, 0x34, 0x56, 0x78}} +
			encodeArgs(u256(-1), 0x60, u256(3), 0x1234567, 0x60, 0x1234, 38, "Lorem ipsum dolor sit ethereum........") +
			vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f4()"), expectation);
	}
}

BOOST_AUTO_TEST_CASE(abi_encode_with_signature)
{
	char const* sourceCode = R"T(
		contract C {
			function f0() public pure returns (bytes memory) {
				return abi.encodeWithSignature("f(uint256)");
			}
			function f1() public pure returns (bytes memory) {
				string memory x = "f(uint256)";
				return abi.encodeWithSignature(x, "abc");
			}
			string xstor;
			function f1s() public returns (bytes memory) {
				xstor = "f(uint256)";
				return abi.encodeWithSignature(xstor, "abc");
			}
			function f2() public pure returns (bytes memory r, uint[] memory ar) {
				string memory x = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
				uint[] memory y = new uint[](4);
				y[0] = type(uint).max;
				y[1] = type(uint).max - 1;
				y[2] = type(uint).max - 2;
				y[3] = type(uint).max - 3;
				r = abi.encodeWithSignature(x, y);
				// The hash uses temporary memory. This allocation re-uses the memory
				// and should initialize it properly.
				ar = new uint[](2);
			}
		}
	)T";
	if (false) {
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f0()"), encodeArgs(0x20, 4, "\xb3\xde\x64\x8b"));
		vector<bytes> expectation;
		expectation = encodeArgs(0x20, 4 + 0x60) + vector<bytes>{{0xb3, 0xde, 0x64, 0x8b}} + encodeArgs(0x20, 3, "abc") + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f1()"), expectation);
		ABI_CHECK(callContractFunction("f1s()"), expectation);
		expectation =
			encodeArgs(0x40, 0x140, 4 + 0xc0) +
			(vector<bytes>{{0xe9, 0xc9, 0x21, 0xcd}} + encodeArgs(0x20, 4, u256(-1), u256(-2), u256(-3), u256(-4)) + vector<bytes>(1, bytes(0x20 - 4))) +
			encodeArgs(2, 0, 0);
		ABI_CHECK(callContractFunction("f2()"), expectation);
	}
}

BOOST_AUTO_TEST_CASE(abi_encode_with_signaturev2)
{
	char const* sourceCode = R"T(
		pragma abicoder v2;
		contract C {
			function f0() public pure returns (bytes memory) {
				return abi.encodeWithSignature("f(uint256)");
			}
			function f1() public pure returns (bytes memory) {
				string memory x = "f(uint256)";
				return abi.encodeWithSignature(x, "abc");
			}
			string xstor;
			function f1s() public returns (bytes memory) {
				xstor = "f(uint256)";
				return abi.encodeWithSignature(xstor, "abc");
			}
			function f2() public pure returns (bytes memory r, uint[] memory ar) {
				string memory x = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
				uint[] memory y = new uint[](4);
				y[0] = type(uint).max;
				y[1] = type(uint).max - 1;
				y[2] = type(uint).max - 2;
				y[3] = type(uint).max - 3;
				r = abi.encodeWithSignature(x, y);
				// The hash uses temporary memory. This allocation re-uses the memory
				// and should initialize it properly.
				ar = new uint[](2);
			}
			struct S { uint a; string b; uint16 c; }
			function f4() public pure returns (bytes memory) {
				bytes4 x = 0x12345678;
				S memory s;
				s.a = 0x1234567;
				s.b = "Lorem ipsum dolor sit ethereum........";
				s.c = 0x1234;
				return abi.encodeWithSignature(s.b, type(uint).max, s, uint(3));
			}
		}
	)T";
	if (false) {
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f0()"), encodeArgs(0x20, 4, "\xb3\xde\x64\x8b"));
		vector<bytes> expectation;
		expectation = encodeArgs(0x20, 4 + 0x60) + vector<bytes>{{0xb3, 0xde, 0x64, 0x8b}} + encodeArgs(0x20, 3, "abc") + vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f1()"), expectation);
		ABI_CHECK(callContractFunction("f1s()"), expectation);
		expectation =
			encodeArgs(0x40, 0x140, 4 + 0xc0) +
			(vector<bytes>{{0xe9, 0xc9, 0x21, 0xcd}} + encodeArgs(0x20, 4, u256(-1), u256(-2), u256(-3), u256(-4)) + vector<bytes>(1, bytes(0x20 - 4))) +
			encodeArgs(2, 0, 0);
		ABI_CHECK(callContractFunction("f2()"), expectation);
		expectation =
			encodeArgs(0x20, 4 + 0x120) +
			vector<bytes>{{0x7c, 0x79, 0x30, 0x02}} +
			encodeArgs(u256(-1), 0x60, u256(3), 0x1234567, 0x60, 0x1234, 38, "Lorem ipsum dolor sit ethereum........") +
			vector<bytes>(1, bytes(0x20 - 4));
		ABI_CHECK(callContractFunction("f4()"), expectation);
	}
}

BOOST_AUTO_TEST_CASE(code_access)
{
	char const* sourceCode = R"(
		contract C {
			function lengths() public pure returns (bool) {
				uint crLen = type(D).creationCode.length;
				uint runLen = type(D).runtimeCode.length;
				require(runLen < crLen);
				require(crLen >= 0x20);
				require(runLen >= 0x20);
				return true;
			}
			function creation() public pure returns (bytes memory) {
				return type(D).creationCode;
			}
			function runtime() public pure returns (bytes memory) {
				return type(D).runtimeCode;
			}
			function runtimeAllocCheck() public pure returns (bytes memory) {
				uint[] memory a = new uint[](2);
				bytes memory c = type(D).runtimeCode;
				uint[] memory b = new uint[](2);
				a[0] = 0x1111;
				a[1] = 0x2222;
				b[0] = 0x3333;
				b[1] = 0x4444;
				return c;
			}
		}
		contract D {
			function f() public pure returns (uint) { return 7; }
		}
	)";
	compileAndRun(sourceCode, 0, "C");
	ABI_CHECK(callContractFunction("lengths()"), encodeArgs(true));
	vector<bytes> codeCreation = callContractFunction("creation()");
	vector<bytes> codeRuntime1 = callContractFunction("runtime()");
	vector<bytes> codeRuntime2 = callContractFunction("runtimeAllocCheck()");
	ABI_CHECK(codeRuntime1, codeRuntime2);
}

BOOST_AUTO_TEST_CASE(contract_name)
{
	char const* sourceCode = R"(
		contract C {
			string public nameAccessor = type(C).name;
			string public constant constantNameAccessor = type(C).name;

			function name() public virtual pure returns (string memory) {
				return type(C).name;
			}
		}
		contract D is C {
			function name() public override pure returns (string memory) {
				return type(D).name;
			}
			function name2() public pure returns (string memory) {
				return type(C).name;
			}
		}
		contract ThisIsAVeryLongContractNameExceeding256bits {
			string public nameAccessor = type(ThisIsAVeryLongContractNameExceeding256bits).name;
			string public constant constantNameAccessor = type(ThisIsAVeryLongContractNameExceeding256bits).name;

			function name() public pure returns (string memory) {
				return type(ThisIsAVeryLongContractNameExceeding256bits).name;
			}
		}
	)";

	compileAndRun(sourceCode, 0, "C");
	vector<bytes> argsC = encodeArgs(encodeDyn(string("C")));
	ABI_CHECK(callContractFunction("name()"), argsC);
	ABI_CHECK(callContractFunction("nameAccessor()"), argsC);
	ABI_CHECK(callContractFunction("constantNameAccessor()"), argsC);

	compileAndRun(sourceCode, 0, "D");
	vector<bytes> argsD = encodeArgs(encodeDyn(string("D")));
	ABI_CHECK(callContractFunction("name()"), argsD);
	ABI_CHECK(callContractFunction("name2()"), argsC);

	string longName = "ThisIsAVeryLongContractNameExceeding256bits";
	compileAndRun(sourceCode, 0, longName);
	vector<bytes> argsLong = encodeArgs(encodeDyn(string(longName)));
	ABI_CHECK(callContractFunction("name()"), argsLong);
	ABI_CHECK(callContractFunction("nameAccessor()"), argsLong);
	ABI_CHECK(callContractFunction("constantNameAccessor()"), argsLong);
}

BOOST_AUTO_TEST_CASE(event_wrong_abi_name)
{
	char const* sourceCode = R"(
		library ClientReceipt {
			event Deposit(Test indexed _from, bytes32 indexed _id, uint256 _value);
			function deposit(bytes32 _id) public {
				Test a;
				emit Deposit(a, _id, msg.value);
			}
		}
		contract Test {
			function f() public {
				ClientReceipt.deposit("123");
			}
		}
	)";
	compileAndRun(sourceCode, 0, "ClientReceipt", vector<bytes>());
	compileAndRun(sourceCode, 0, "Test", vector<bytes>(), map<string, h160>{{"ClientReceipt", m_contractAddress}});

	callContractFunction("f()");
	BOOST_REQUIRE_EQUAL(numLogs(), 1);
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress);
	BOOST_REQUIRE_EQUAL(numLogTopics(0), 3);
	BOOST_CHECK_EQUAL(logTopic(0, 0), util::keccak256(string("Deposit(address,bytes32,uint256)")));
}

BOOST_AUTO_TEST_CASE(uninitialized_internal_storage_function)
{
	char const* sourceCode = R"(
		contract Test {
			function() internal x;
			function f() public returns (uint r) {
				function() internal t = x;
				assembly { r := t }
			}
		}
	)";
	if (false) {
		compileAndRun(sourceCode, 0, "Test");

		vector<bytes> result = callContractFunction("f()");
		BOOST_CHECK(!result.empty());
		BOOST_CHECK(result != encodeArgs(0));
	}
}

BOOST_AUTO_TEST_CASE(dirty_scratch_space_prior_to_constant_optimiser)
{
	char const* sourceCode = R"(
		contract C {
			event X(uint);
			constructor() {
				assembly {
					// make scratch space dirty
					mstore(0, 0x4242424242424242424242424242424242424242424242424242424242424242)
				}
				uint x = 0x0000000000001234123412431234123412412342112341234124312341234124;
				// This is just to create many instances of x
				unchecked { emit X(x + f() * g(tx.origin) ^ h(block.number)); }
				assembly {
					// make scratch space dirty
					mstore(0, 0x4242424242424242424242424242424242424242424242424242424242424242)
				}
				emit X(x);
			}
			function f() internal pure returns (uint) {
				return 0x0000000000001234123412431234123412412342112341234124312341234124;
			}
			function g(address a) internal pure returns (uint) {
				unchecked { return uint(uint160(a)) * 0x0000000000001234123412431234123412412342112341234124312341234124; }
			}
			function h(uint a) internal pure returns (uint) {
				unchecked { return a * 0x0000000000001234123412431234123412412342112341234124312341234124; }
			}
		}
	)";
	if (false) {
	compileAndRun(sourceCode, 0, "C");
		BOOST_REQUIRE_EQUAL(numLogs(), 2);
		BOOST_CHECK_EQUAL(logAddress(1), m_contractAddress);
		ABI_CHECK(
			logData(1),
			encodeLogs(u256("0x0000000000001234123412431234123412412342112341234124312341234124"))
		);
	}
}

BOOST_AUTO_TEST_CASE(strip_reason_strings)
{
	char const* sourceCode = R"(
		contract C {
			function f(bool _x) public pure returns (uint) {
				require(_x, "some reason");
				return 7;
			}
			function g(bool _x) public pure returns (uint) {
				string memory x = "some indirect reason";
				require(_x, x);
				return 8;
			}
			function f1(bool _x) public pure returns (uint) {
				if (!_x) revert( /* */ "some reason" /* */ );
				return 9;
			}
			function g1(bool _x) public pure returns (uint) {
				string memory x = "some indirect reason";
				if (!_x) revert(x);
				return 10;
			}
		}
	)";
	m_revertStrings = RevertStrings::Default;
	compileAndRun(sourceCode, 0, "C");

	if (
		m_optimiserSettings == OptimiserSettings::minimal() ||
		m_optimiserSettings == OptimiserSettings::none()
	)
		// check that the reason string IS part of the binary.
		BOOST_CHECK(m_output.size() > 0 && toHex(m_output[0]).find("6e6f7361657220656d6f73") != std::string::npos);

	m_revertStrings = RevertStrings::Strip;
	compileAndRun(sourceCode, 0, "C");
	// check that the reason string is NOT part of the binary.
	//BOOST_CHECK(m_output.size() > 0 && toHex(m_output[0]).find("6e6f7361657220656d6f73") == std::string::npos);

	ABI_CHECK(callContractFunction("f(bool)", true), encodeArgs(7));
	ABI_CHECK(callContractFunction("f(bool)", false), encodeArgs());
	ABI_CHECK(callContractFunction("g(bool)", true), encodeArgs(8));
	ABI_CHECK(callContractFunction("g(bool)", false), encodeArgs());
	ABI_CHECK(callContractFunction("f1(bool)", true), encodeArgs(9));
	ABI_CHECK(callContractFunction("f1(bool)", false), encodeArgs());
	ABI_CHECK(callContractFunction("g1(bool)", true), encodeArgs(10));
	ABI_CHECK(callContractFunction("g1(bool)", false), encodeArgs());
}

BOOST_AUTO_TEST_CASE(assert_and_require)
{
	char const* sourceCode = R"(
		contract Test {
			function foo() public returns (int) {
				require(true);
				assert(false);
			}
		}	
	)";
	compileAndRun(sourceCode);
	BOOST_CHECK(callContractFunction("foo()") == encodeArgs());
	BOOST_CHECK(m_status == 255);
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
