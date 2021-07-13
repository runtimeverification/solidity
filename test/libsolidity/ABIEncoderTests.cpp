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
 * Unit tests for Solidity's ABI encoder.
 */

#include <test/libsolidity/SolidityExecutionFramework.h>

#include <test/libsolidity/ABITestsCommon.h>

#include <liblangutil/Exceptions.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/test/unit_test.hpp>

#include <functional>
#include <string>
#include <tuple>

using namespace std;
using namespace std::placeholders;
using namespace solidity::util;
using namespace solidity::test;

namespace solidity::frontend::test
{

#define REQUIRE_LOG_DATA(DATA) do { \
	BOOST_REQUIRE_EQUAL(numLogs(), 1); \
	BOOST_CHECK_EQUAL(logAddress(0), m_contractAddress); \
	ABI_CHECK(logData(0), DATA); \
} while (false)

BOOST_FIXTURE_TEST_SUITE(ABIEncoderTest, SolidityExecutionFramework)

BOOST_AUTO_TEST_CASE(value_types)
{
	string sourceCode = R"(
		contract C {
			event E(uint a, uint16 b, uint24 c, int24 d, bytes3 x, bool, C);
			function f() public {
				bytes6 x = hex"1bababababa2";
				bool b = true;
				C c = C(address(uint160(int160(-5))));
				emit E(10, uint16(type(uint256).max - 1), uint24(uint(0x12121212)), int24(int256(-1)), bytes3(x), b, c);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			bigint(10), int16_t(65534), fromHex("121212"), fromHex("ffffff"), string("\x1b\xab\xab"), true, u160(h160("fffffffffffffffffffffffffffffffffffffffb"))
		));
	)
}

BOOST_AUTO_TEST_CASE(string_literal)
{
	string sourceCode = R"(
		contract C {
			event E(string, bytes20, string);
			function f() public {
				emit E("abcdef", "abcde", "abcdefabcdefgehabcabcasdfjklabcdefabcedefghabcabcasdfjklabcdefabcdefghabcabcasdfjklabcdeefabcdefghabcabcasdefjklabcdefabcdefghabcabcasdfjkl");
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			uint64_t(6), string("abcdef"), string("abcde") + string(15, 0), 
			uint64_t(0x8b), string("abcdefabcdefgehabcabcasdfjklabcdefabcedefghabcabcasdfjklabcdefabcdefghabcabcasdfjklabcdeefabcdefghabcabcasdefjklabcdefabcdefghabcabcasdfjkl")
		));
	)
}


BOOST_AUTO_TEST_CASE(enum_type_cleanup)
{
	string sourceCode = R"(
		contract C {
			enum E { A, B }
			function f(uint x) public returns (E en) {
				en = E(x);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		BOOST_CHECK(callContractFunction("f(uint)", 0) == encodeArgs(0));
		BOOST_CHECK(callContractFunction("f(uint)", 1) == encodeArgs(1));
		BOOST_CHECK(callContractFunction("f(uint)", 2) == encodeArgs());
	)
}

BOOST_AUTO_TEST_CASE(conversion)
{
	string sourceCode = R"(
		contract C {
			event E(bytes4, bytes4, uint16, uint8, int16, int8);
			function f() public {
                bytes4 x0 = 0xf1f2f3f4;
				bytes2 x = bytes2(x0);
				uint8 a;
				uint16 b = 0x1ff;
				int8 c;
				int16 d;
                a = uint8(int8(0 - 1));
                c = int8(int24(0x0101ff));
                d = int16(uint16(0xff01));
				emit E(bytes4(uint32(10)), x, a, uint8(b), c, int8(d));
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			string(3, 0) + string("\x0a"), string("\xf1\xf2") + string(2, 0),
			int16_t(0xff), int8_t(0xff), int16_t(-1), 1
		));
	)
}

BOOST_AUTO_TEST_CASE(memory_array_one_dim)
{
	string sourceCode = R"(
		contract C {
			event E(uint a, int16[] b, uint c);
			function f() public {
				int16[] memory x = new int16[](3);
				for (uint i = 0; i < 3; i++) {
					x[i] = int16(int(0xfffffffe + i));
				}
				emit E(10, x, 11);
			}
		}
	)";

	if (solidity::test::CommonOptions::get().useABIEncoderV1)
	{
		compileAndRun(sourceCode);
		callContractFunction("f()");
		// The old encoder does not clean array elements.
		REQUIRE_LOG_DATA(encodeLogs(bigint(10), 1, 3, int16_t(-2), int16_t(-1), int16_t(0), bigint(11)));
	}

	NEW_ENCODER(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(bigint(10), 1, 3, int16_t(-2), int16_t(-1), int16_t(0), bigint(11)));
	)
}

BOOST_AUTO_TEST_CASE(memory_array_two_dim)
{
	string sourceCode = R"(
		contract C {
			event E(uint a, int16[][2] b, uint c);
			function f() public {
				int16[][2] memory x;
				x[0] = new int16[](3);
				x[1] = new int16[](2);
				x[0][0] = 7;
				x[0][1] = int16(int(0x010203040506));
				x[0][2] = -1;
				x[1][0] = 4;
				x[1][1] = 5;
				emit E(10, x, 11);
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(bigint(10), 1, 3, int16_t(7), 0x0506, int16_t(-1), 1, 2, int16_t(4), int16_t(5), bigint(11)));
	)
}

BOOST_AUTO_TEST_CASE(memory_byte_array)
{
	string sourceCode = R"(
		contract C {
			event E(uint a, bytes[] b, uint c);
			function f() public {
				bytes[] memory x = new bytes[](2);
				x[0] = "abcabcdefghjklmnopqrsuvwabcdefgijklmnopqrstuwabcdefgijklmnoprstuvw";
				x[1] = "abcdefghijklmnopqrtuvwabcfghijklmnopqstuvwabcdeghijklmopqrstuvw";
				emit E(10, x, 11);
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			bigint(10), 1, 2,
			uint64_t(66), string("abcabcdefghjklmnopqrsuvwabcdefgijklmnopqrstuwabcdefgijklmnoprstuvw"),
			uint64_t(63), string("abcdefghijklmnopqrtuvwabcfghijklmnopqstuvwabcdeghijklmopqrstuvw"),
                        bigint(11)
		));
	)
}

BOOST_AUTO_TEST_CASE(storage_byte_array)
{
	string sourceCode = R"(
		contract C {
			bytes short;
			bytes long;
			event E(bytes s, bytes l);
			function f() public {
				short = "123456789012345678901234567890a";
				long = "ffff123456789012345678901234567890afffffffff123456789012345678901234567890a";
				emit E(short, long);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			uint64_t(31), string("123456789012345678901234567890a"),
			uint64_t(75), string("ffff123456789012345678901234567890afffffffff123456789012345678901234567890a")
		));
	)
}

BOOST_AUTO_TEST_CASE(storage_array)
{
	string sourceCode = R"(
		contract C {
			address[3] addr;
			event E(address[3] a);
			function f() public {
				addr[0] = address(uint160(int160(-1)));
				addr[1] = address(uint160(int160(-2)));
				addr[2] = address(uint160(int160(-3)));
				emit E(addr);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			u160(h160("ffffffffffffffffffffffffffffffffffffffff")),
			u160(h160("fffffffffffffffffffffffffffffffffffffffe")),
			u160(h160("fffffffffffffffffffffffffffffffffffffffd"))
		));
	)
}

BOOST_AUTO_TEST_CASE(storage_array_dyn)
{
	string sourceCode = R"(
		contract C {
			address[] addr;
			event E(address[] a);
			function f() public {
				addr.push(0x0000000000000000000000000000000000000001);
				addr.push(0x0000000000000000000000000000000000000002);
				addr.push(0x0000000000000000000000000000000000000003);
				emit E(addr);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			1,
			3,
			u160(h160("0000000000000000000000000000000000000001")),
			u160(h160("0000000000000000000000000000000000000002")),
			u160(h160("0000000000000000000000000000000000000003"))
		));
	)
}

BOOST_AUTO_TEST_CASE(storage_array_compact)
{
	string sourceCode = R"(
		contract C {
			int72[] x;
			event E(int72[]);
			function f() public {
				x.push(-1);
				x.push(2);
				x.push(-3);
				x.push(4);
				x.push(-5);
				x.push(6);
				x.push(-7);
				x.push(8);
				emit E(x);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f()");
		REQUIRE_LOG_DATA(encodeLogs(
			1, 8, s72(-1), s72(2), s72(-3), s72(4), s72(-5), s72(6), s72(-7), s72(8)
		));
	)
}

BOOST_AUTO_TEST_CASE(external_function)
{
	string sourceCode = R"(
		contract C {
			event E(function(uint) external returns (uint), function(uint) external returns (uint));
			function(uint) external returns (uint) g;
			function f(uint) public returns (uint) {
				g = this.f;
				emit E(this.f, g);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f(uint)", u256(0));
		REQUIRE_LOG_DATA(encodeLogs(m_contractAddress, int16_t(1), m_contractAddress, int16_t(1)));
	)
}

BOOST_AUTO_TEST_CASE(external_function_cleanup)
{
	string sourceCode = R"(
		contract C {
			event E(function(uint) external returns (uint), function(uint) external returns (uint));
			// This test relies on the fact that g is stored in slot zero.
			function(uint) external returns (uint) g;
			function f(uint) public returns (uint) {
				function(uint) external returns (uint)[1] memory h;
				assembly { sstore(0, sub(0, 1)) mstore(h, sub(0, 1)) }
				emit E(h[0], g);
			}
		}
	)";
	BOTH_ENCODERS(
		if (false) {
			compileAndRun(sourceCode, 0, "C");
			callContractFunction("f(uint256)", u256(0));
			REQUIRE_LOG_DATA(encodeLogs(string(24, char(-1)), string(24, char(-1))));
		}
	)
}

BOOST_AUTO_TEST_CASE(calldata)
{
	string sourceCode = R"(
		contract C {
			event E(bytes);
			function f(bytes calldata a) external {
				emit E(a);
			}
		}
	)";
	string s("abcdef");
	string t("abcdefgggggggggggggggggggggggggggggggggggggggghhheeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeggg");
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		callContractFunction("f(bytes)", encodeArgs(encodeDyn(s)));
		// The old encoder did not pad to multiples of 32 bytes
		REQUIRE_LOG_DATA(encodeLogs(uint64_t(s.size()), s));
		callContractFunction("f(bytes)", encodeArgs(encodeDyn(t)));
		REQUIRE_LOG_DATA(encodeLogs(uint64_t(t.size()), t));
	)
}

BOOST_AUTO_TEST_CASE(function_name_collision)
{
	// This tests a collision between a function name used by inline assembly
	// and by the ABI encoder
	string sourceCode = R"(
		contract C {
			function f(uint x) public returns (uint) {
				assembly {
					function abi_encode_t_uint256_to_t_uint256() {
						mstore(0, 7)
						return(0, 0x20)
					}
					switch x
					case 0 { abi_encode_t_uint256_to_t_uint256() }
				}
				return 1;
			}
		}
	)";
	BOTH_ENCODERS(
		if (false) {
			compileAndRun(sourceCode, 0, "C");
			BOOST_CHECK(callContractFunction("f(uint256)", encodeArgs(0)) == encodeArgs(7));
			BOOST_CHECK(callContractFunction("f(uint256)", encodeArgs(1)) == encodeArgs(1));
		}
	)
}

BOOST_AUTO_TEST_CASE(structs)
{
	string sourceCode = R"(
		contract C {
			struct S { uint16 a; uint16 b; T[] sub; uint16 c; }
			struct T { uint64[2] x; }
			S s;
			event e(uint16, S);
			function f() public returns (uint, S memory) {
				uint16 x = 7;
				s.a = 8;
				s.b = 9;
				s.c = 10;
				s.sub.push();
				s.sub.push();
				s.sub.push();
				s.sub[0].x[0] = 11;
				s.sub[1].x[0] = 12;
				s.sub[2].x[1] = 13;
				emit e(x, s);
				return (x, s);
			}
		}
	)";

	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		std::vector<bytes> encoded = encodeArgs(
			u256(7), encodeRefArgs(
			int16_t(8), int16_t(9), 1, 3, uint64_t(11), uint64_t(0), uint64_t(12), uint64_t(0), uint64_t(0), uint64_t(13), uint8_t(10)
		));
		bytes flattened = encodeLogs(int16_t(7), int16_t(8), int16_t(9), 1, 3, uint64_t(11), uint64_t(0), uint64_t(12), uint64_t(0), uint64_t(0), uint64_t(13), int16_t(10));
		ABI_CHECK(callContractFunction("f()"), encoded);
		REQUIRE_LOG_DATA(flattened);
		BOOST_CHECK_EQUAL(logTopic(0, 0), keccak256(string("e(uint16,(uint16,uint16,(uint64[2])[],uint16))")));
	)
}

BOOST_AUTO_TEST_CASE(structs2)
{
	string sourceCode = R"(
		contract C {
			enum E {A, B, C}
			struct T { uint x; E e; uint8 y; }
			struct S { C c; T[] t;}
			function f() public returns (uint a, S[2] memory s1, S[] memory s2, uint b) {
				a = 7;
				b = 8;
				s1[0].c = this;
				s1[0].t = new T[](1);
				s1[0].t[0].x = 0x11;
				s1[0].t[0].e = E.B;
				s1[0].t[0].y = 0x12;
				s2 = new S[](2);
				s2[1].c = C(address(0x1234));
				s2[1].t = new T[](3);
				s2[1].t[1].x = 0x21;
				s2[1].t[1].e = E.C;
				s2[1].t[1].y = 0x22;
			}
		}
	)";

	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f()"), encodeArgs(
			7, encodeRefArgs(
			// S[2] s1
			// S s1[0]
			u160(m_contractAddress),
			// T s1[0].t
			1, 1, // length
			// s1[0].t[0]
			bigint(0x11), 1, 0x12,
			// S s1[1]
			u160(0),
			// T s1[1].t
			1), encodeRefArgs(
			// S[] s2 (0x1e0)
			1, 2, // length
			// S s2[0]
			u160(0), 1, 0,
			// S s2[1]
			u160(0x1234),
			// s2[1].t
			1, 3, // length
			bigint(0), 0, 0,
			bigint(0x21), 2, 0x22,
			1),
                        8
		));
	)
}

BOOST_AUTO_TEST_CASE(bool_arrays)
{
	string sourceCode = R"(
		contract C {
			bool[] x;
			bool[4] y;
			event E(bool[], bool[4]);
			function f() public returns (bool[] memory, bool[4] memory) {
				x.push(true);
				x.push(false);
				x.push(true);
				x.push(false);
				y[0] = true;
				y[1] = false;
				y[2] = true;
				y[3] = false;
				emit E(x, y);
				return (x, y); // this copies to memory first
			}
		}
	)";

	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		vector<bytes> encoded = encodeArgs(
			encodeRefArray({true, false, true, false}, 4, 1),
			encodeRefArray({true, false, true, false}, 1)
		);
		bytes flattened = encodeLogs(
			1, 4, true, false, true, false,
			true, false, true, false
		);
		ABI_CHECK(callContractFunction("f()"), encoded);
		REQUIRE_LOG_DATA(flattened);
	)
}

BOOST_AUTO_TEST_CASE(bool_arrays_split)
{
	string sourceCode = R"(
		contract C {
			bool[] x;
			bool[4] y;
			event E(bool[], bool[4]);
			function store() public {
				x.push(true);
				x.push(false);
				x.push(true);
				x.push(false);
				y[0] = true;
				y[1] = false;
				y[2] = true;
				y[3] = false;
			}
			function f() public returns (bool[] memory, bool[4] memory) {
				emit E(x, y);
				return (x, y); // this copies to memory first
			}
		}
	)";

	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		vector<bytes> encoded = encodeArgs(
			encodeRefArray({true, false, true, false}, 4, 1),
			encodeRefArray({true, false, true, false}, 1)
		);
		bytes flattened = encodeLogs(
			1, 4, true, false, true, false,
			true, false, true, false
		);
		ABI_CHECK(callContractFunction("store()"), vector<bytes>{});
		ABI_CHECK(callContractFunction("f()"), encoded);
		REQUIRE_LOG_DATA(flattened);
	)
}

BOOST_AUTO_TEST_CASE(bytesNN_arrays)
{
	// This tests that encoding packed arrays from storage work correctly.
	string sourceCode = R"(
		contract C {
			bytes8[] x;
			bytesWIDTH[SIZE] y;
			event E(bytes8[], bytesWIDTH[SIZE]);
			function store() public {
				x.push("abc");
				x.push("def");
				for (uint i = 0; i < y.length; i ++)
					y[i] = bytesWIDTH(uintUINTWIDTH(i + 1));
			}
			function f() public returns (bytes8[] memory, bytesWIDTH[SIZE] memory) {
				emit E(x, y);
				return (x, y); // this copies to memory first
			}
		}
	)";

	BOTH_ENCODERS(
		//for (size_t size = 1; size < 15; size++)
		for (size_t size = 1; size < 4; size++)
		{
			for (size_t width: {1u, 2u, 4u, 5u, 7u, 15u, 16u, 17u, 31u, 32u})
			{
				string source = boost::algorithm::replace_all_copy(sourceCode, "SIZE", to_string(size));
				source = boost::algorithm::replace_all_copy(source, "UINTWIDTH", to_string(width * 8));
				source = boost::algorithm::replace_all_copy(source, "WIDTH", to_string(width));
				compileAndRun(source, 0, "C");
				ABI_CHECK(callContractFunction("store()"), vector<bytes>{});
				vector<u256> arr;
				for (size_t i = 0; i < size; i ++)
					arr.emplace_back(u256(i + 1));
				vector<bytes> encoded = encodeArgs(
					encodeRefArray({0x6162630000000000, 0x6465660000000000}, 2, 8),
					encodeRefArray(arr, width)
				);
				bytes flattened = encodeLogs(
					1, 2, uint64_t(0x6162630000000000), uint64_t(0x6465660000000000)
				);
				for (size_t i = 0; i < size; i ++)
					flattened += bytes{uint8_t(arr[i])} + bytes(width - 1, 0);
				ABI_CHECK(callContractFunction("f()"), encoded);
				REQUIRE_LOG_DATA(flattened);
			}
		}
	)
}

BOOST_AUTO_TEST_CASE(bytesNN_arrays_dyn)
{
	// This tests that encoding packed arrays from storage work correctly.
	string sourceCode = R"(
		contract C {
			bytes8[] x;
			bytesWIDTH[] y;
			event E(bytesWIDTH[], bytes8[]);
			function store() public {
				x.push("abc");
				x.push("def");
				for (uint i = 0; i < SIZE; i ++)
					y.push(bytesWIDTH(uintUINTWIDTH(i + 1)));
			}
			function f() public returns (bytesWIDTH[] memory, bytes8[] memory) {
				emit E(y, x);
				return (y, x); // this copies to memory first
			}
		}
	)";

	BOTH_ENCODERS(
		//for (size_t size = 0; size < 15; size++)
		for (size_t size = 1; size < 4; size++)
		{
			for (size_t width: {1u, 2u, 4u, 5u, 7u, 15u, 16u, 17u, 31u, 32u})
			{
				string source = boost::algorithm::replace_all_copy(sourceCode, "SIZE", to_string(size));
				source = boost::algorithm::replace_all_copy(source, "UINTWIDTH", to_string(width * 8));
				source = boost::algorithm::replace_all_copy(source, "WIDTH", to_string(width));
				compileAndRun(source, 0, "C");
				ABI_CHECK(callContractFunction("store()"), vector<bytes>{});
				vector<u256> arr;
				for (size_t i = 0; i < size; i ++)
					arr.emplace_back(u256(i + 1));
				vector<bytes> encoded = encodeArgs(
					encodeRefArray(arr, size, width),
					encodeRefArray({0x6162630000000000, 0x6465660000000000}, 2, 8)
				);
				bytes flattened = encodeLogs(1, int(size));
				for (size_t i = 0; i < size; i ++)
					flattened += bytes{uint8_t(arr[i])} + bytes(width - 1, 0);
				flattened += encodeLogs(
					1, 2, uint64_t(0x6162630000000000), uint64_t(0x6465660000000000)
				);
				ABI_CHECK(callContractFunction("f()"), encoded);
				REQUIRE_LOG_DATA(flattened);
			}
		}
	)
}

BOOST_AUTO_TEST_CASE(packed_structs)
{
	string sourceCode = R"(
		contract C {
			struct S { bool a; int8 b; function() external g; bytes3 d; int8 e; }
			S s;
			event E(S);
			function store() public {
				s.a = false;
				s.b = -5;
				s.g = this.g;
				s.d = 0x010203;
				s.e = -3;
			}
			function f() public returns (S memory) {
				emit E(s);
				return s; // this copies to memory first
			}
			function g() public pure {}
		}
	)";

	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("store()"), vector<bytes>{});
		vector<bytes> encoded = encodeArgs(encodeRefArgs(
			false, int8_t(-5), u160(m_contractAddress), int16_t(4), "\x01\x02\x03", int8_t(-3)
		));
		bytes flattened = encodeLogs(
			false, int8_t(-5), u160(m_contractAddress), int16_t(4), "\x01\x02\x03", int8_t(-3)
		);
		ABI_CHECK(callContractFunction("f()"), encoded);
		REQUIRE_LOG_DATA(flattened);
	)
}


BOOST_AUTO_TEST_CASE(struct_in_constructor)
{
	string sourceCode = R"(
		contract C {
			struct S {
				string a;
				uint8 b;
				string c;
			}
			S public x;
			constructor(S memory s) { x = s; }
		}
	)";

	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C", encodeArgs(encodeRefArgs(encodeDyn(string("abc")), 0x03, encodeDyn(string("def")))));
		ABI_CHECK(callContractFunction("x()"), encodeArgs(encodeDyn(string("abc")), 0x03, encodeDyn(string("def"))));
	)
}

BOOST_AUTO_TEST_CASE(struct_in_constructor_indirect)
{
	string sourceCode = R"(
		contract C {
			struct S {
				string a;
				uint8 b;
				string c;
			}
			S public x;
			constructor(S memory s) { x = s; }
		}

		contract D {
			function f() public returns (string memory, uint8, string memory) {
				C.S memory s;
				s.a = "abc";
				s.b = 7;
				s.c = "def";
				C c = new C(s);
				return c.x();
			}
		}
	)";
	if (solidity::test::CommonOptions::get().evmVersion().supportsReturndata())
	{
		NEW_ENCODER(
			compileAndRun(sourceCode, 0, "D");
			ABI_CHECK(callContractFunction("f()"), encodeArgs(encodeDyn(string("abc")), 7, encodeDyn(string("def"))));
		)
	}
}

BOOST_AUTO_TEST_CASE(struct_in_constructor_data_short)
{
	string sourceCode = R"(
		contract C {
			struct S {
				string a;
				uint8 b;
				string c;
			}
			S public x;
			constructor(S memory s) { x = s; }
		}
	)";

	NEW_ENCODER(
		BOOST_CHECK(
			!compileAndRunWithoutCheck({{"", sourceCode}}, 0, "C", false, encodeArgs(encodeRefArgs(encodeDyn(string("abc")), 0x03))).empty()
		);
	)
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
