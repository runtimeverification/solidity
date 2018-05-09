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
 * Unit tests for Solidity's ABI decoder.
 */

#include <functional>
#include <string>
#include <tuple>
#include <boost/test/unit_test.hpp>
#include <libsolidity/interface/Exceptions.h>
#include <test/libsolidity/SolidityExecutionFramework.h>

#include <test/libsolidity/ABITestsCommon.h>

using namespace std;
using namespace std::placeholders;
using namespace dev::test;

namespace dev
{
namespace solidity
{
namespace test
{

BOOST_FIXTURE_TEST_SUITE(ABIDecoderTest, SolidityExecutionFramework)

BOOST_AUTO_TEST_CASE(both_encoders_macro)
{
	// This tests that the "both decoders macro" at least runs twice and
	// modifies the source.
	string sourceCode;
	int runs = 0;
	BOTH_ENCODERS(runs++;)
	BOOST_CHECK(sourceCode == NewEncoderPragma);
	BOOST_CHECK_EQUAL(runs, 2);
}

BOOST_AUTO_TEST_CASE(value_types)
{
	string sourceCode = R"(
		contract C {
			function f(uint a, uint16 b, uint24 c, int24 d, bytes3 x, bool e, C g) public returns (uint) {
				if (a != 1) return 1;
				if (b != 2) return 2;
				if (c != 3) return 3;
				if (d != 4) return 4;
				if (x != "abc") return 5;
				if (e != true) return 6;
				if (g != this) return 7;
				return 20;
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction(
			"f(uint,uint16,uint24,int24,bytes3,bool,address)",
			1, 2, 3, 4, string("abc"), true, u160(m_contractAddress)
		), encodeArgs(u256(20)));
	)
}

BOOST_AUTO_TEST_CASE(enums)
{
	string sourceCode = R"(
		contract C {
			enum E { A, B }
			function f(E e) public pure returns (uint x) {
				x = uint(e);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction("f(uint8)", 0), encodeArgs(u256(0)));
		ABI_CHECK(callContractFunction("f(uint8)", 1), encodeArgs(u256(1)));
		// The old decoder was not as strict about enums
		ABI_CHECK(callContractFunction("f(uint8)", 2), encodeArgs());
		ABI_CHECK(callContractFunction("f(uint8)", -1), encodeArgs());
	)
}

BOOST_AUTO_TEST_CASE(cleanup)
{
	string sourceCode = R"(
		contract C {
			function f(uint16 a, int16 b, address c, bytes3 d, bool e)
					public pure returns (uint v, uint w, uint x, uint y, uint z) {
				v = a; w = uint256(b); x = uint256(c); y = uint256(d); z = e ? 1 : 0;
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(
			callContractFunction("f(uint16,int16,address,bytes3,bool)", 1, 2, 3, "a", true),
			encodeArgs(u256(1), u256(2), u256(3), string("a"), true)
		);
		ABI_CHECK(
			callContractFunction(
				"f(uint16,int16,address,bytes3,bool)",
				u256(0xffffff), u256(0x1ffff), u256(-1), string("abcd"), u256(4)
			),
			encodeArgs());
		ABI_CHECK(
			callContractFunction(
				"f(uint16,int16,address,bytes3,bool)",
				u256(0xffff), -1, u160(-1), string("abc"), u256(1)
			),
			encodeArgs(u256(0xffff), u256(-1), (u256(1) << 160) - 1, string("abc"), true)
		);
	)
}

BOOST_AUTO_TEST_CASE(fixed_arrays)
{
	string sourceCode = R"(
		contract C {
			function f(uint16[3] a, uint16[2][3] b, uint i, uint j, uint k)
					public pure returns (uint, uint) {
				return (a[i], b[j][k]);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(
			callContractFunction("f(uint16[3],uint16[2][3],uint,uint,uint)", 
			encodeRefArgs(int16_t(1), int16_t(2), byte(3)),
			encodeRefArgs(int16_t(11), int16_t(12),
			int16_t(21), int16_t(22),
			int16_t(31), byte(32)),
			1, 2, 1
			),
			encodeArgs(u256(2), u256(32))
		);
	)
}

BOOST_AUTO_TEST_CASE(dynamic_arrays)
{
	string sourceCode = R"(
		contract C {
			function f(uint a, uint16[] b, uint c)
					public pure returns (uint, uint, uint) {
				return (b.length, b[a], c);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(
			callContractFunction("f(uint,uint16[],uint)",
			6, encodeRefArgs(1, 7,
			int16_t(11), int16_t(12), int16_t(13), int16_t(14), int16_t(15), int16_t(16), byte(17)),
			9
			),
			encodeArgs(u256(7), u256(17), u256(9))
		);
	)
}

BOOST_AUTO_TEST_CASE(dynamic_nested_arrays)
{
	string sourceCode = R"(
		contract C {
			function f(uint a, uint16[][] b, uint[2][][3] c, uint d)
					public pure returns (uint, uint, uint, uint, uint, uint, uint) {
				return (a, b.length, b[1].length, b[1][1], c[1].length, c[1][1][1], d);
			}
			function test() view returns (uint, uint, uint, uint, uint, uint, uint) {
				uint16[][] memory b = new uint16[][](3);
				b[0] = new uint16[](2);
				b[0][0] = 0x55;
				b[0][1] = 0x56;
				b[1] = new uint16[](4);
				b[1][0] = 0x65;
				b[1][1] = 0x66;
				b[1][2] = 0x67;
				b[1][3] = 0x68;

				uint[2][][3] memory c;
				c[0] = new uint[2][](1);
				c[0][0][1] = 0x75;
				c[1] = new uint[2][](5);
				c[1][1][1] = 0x85;

				return this.f(0x12, b, c, 0x13);
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode);
		std::vector<bytes> args = encodeArgs(
		);

		std::vector<bytes> expectation = encodeArgs(0x12, 3, 4, 0x66, 5, 0x85, 0x13);
		ABI_CHECK(callContractFunction("test()"), expectation);
		ABI_CHECK(callContractFunction("f(uint,uint16[][],uint[2][][3],uint)",
			0x12, encodeRefArgs(
			// b
			1, 3, 
			1, 2, int16_t(85), int16_t(86),
			1, 4, int16_t(101), int16_t(102), int16_t(103), int16_t(104),
			1, 0), encodeRefArgs(
			// c
			1, 1, bigint(0), bigint(117),
			1, 5, bigint(0), bigint(0), bigint(0), bigint(133), bigint(0), bigint(0), bigint(0), bigint(0), bigint(0), bigint(0),
			1, 0), 0x13
		), expectation);
	)
}

BOOST_AUTO_TEST_CASE(byte_arrays)
{
	string sourceCode = R"(
		contract C {
			function f(uint a, bytes b, uint c)
					public pure returns (uint, uint, byte, uint) {
				return (a, b.length, b[3], c);
			}

			function f_external(uint a, bytes b, uint c)
					external pure returns (uint, uint, byte, uint) {
				return (a, b.length, b[3], c);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		std::vector<bytes> args = encodeArgs(
			6, encodeDyn(string("abcdefg")), 9
		);
		ABI_CHECK(
			callContractFunctionNoEncoding("f(uint,bytes,uint)", args),
			encodeArgs(u256(6), u256(7), "d", 9)
		);
		ABI_CHECK(
			callContractFunctionNoEncoding("f_external(uint,bytes,uint)", args),
			encodeArgs(u256(6), u256(7), "d", 9)
		);
	)
}

BOOST_AUTO_TEST_CASE(calldata_arrays_too_large)
{
	string sourceCode = R"(
		contract C {
			function f(uint a, uint[] b, uint c) external pure returns (uint) {
				return 7;
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(
			callContractFunction("f(uint,uint[],uint)",
				6, encodeRefArgs(32,
				(u256(1) << 255) + 2, bigint(1), bigint(2)), 9),
			encodeArgs()
		);
	)
}

BOOST_AUTO_TEST_CASE(decode_from_memory_simple)
{
	string sourceCode = R"(
		contract C {
			uint public _a;
			uint[] public _b;
			function C(uint a, uint[] b) {
				_a = a;
				_b = b;
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C", encodeArgs(
			7, encodeRefArgs(1,
			// b
			3, bigint(0x21), bigint(0x22), bigint(0x23)
		)));
		ABI_CHECK(callContractFunction("_a()"), encodeArgs(7));
		ABI_CHECK(callContractFunction("_b(uint)", 0), encodeArgs(0x21));
		ABI_CHECK(callContractFunction("_b(uint)", 1), encodeArgs(0x22));
		ABI_CHECK(callContractFunction("_b(uint)", 2), encodeArgs(0x23));
		ABI_CHECK(callContractFunction("_b(uint)", 3), encodeArgs());
	)
}

BOOST_AUTO_TEST_CASE(decode_function_type)
{
	string sourceCode = R"(
		contract D {
			function () external returns (uint) public _a;
			function D(function () external returns (uint) a) {
				_a = a;
			}
		}
		contract C {
			function f() returns (uint) {
				return 3;
			}
			function g(function () external returns (uint) _f) returns (uint) {
				return _f();
			}
			// uses "decode from memory"
			function test1() returns (uint) {
				D d = new D(this.f);
				return d._a()();
			}
			// uses "decode from calldata"
			function test2() returns (uint) {
				return this.g(this.f);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("test1()"), encodeArgs(3));
		ABI_CHECK(callContractFunction("test2()"), encodeArgs(3));
	)
}

BOOST_AUTO_TEST_CASE(decode_function_type_array)
{
	string sourceCode = R"(
		contract D {
			function () external returns (uint)[] public _a;
			function D(function () external returns (uint)[] a) {
				_a = a;
			}
		}
		contract E {
			function () external returns (uint)[3] public _a;
			function E(function () external returns (uint)[3] a) {
				_a = a;
			}
		}
		contract C {
			function f1() public returns (uint) {
				return 1;
			}
			function f2() public returns (uint) {
				return 2;
			}
			function f3() public returns (uint) {
				return 3;
			}
			function g(function () external returns (uint)[] _f, uint i) public returns (uint) {
				return _f[i]();
			}
			function h(function () external returns (uint)[3] _f, uint i) public returns (uint) {
				return _f[i]();
			}
			// uses "decode from memory"
			function test1_dynamic() public returns (uint) {
				var x = new function() external returns (uint)[](3);
				x[0] = this.f1;
				x[1] = this.f2;
				x[2] = this.f3;
				D d = new D(x);
				return d._a(2)();
			}
			function test1_static() public returns (uint) {
				E e = new E([this.f1, this.f2, this.f3]);
				return e._a(2)();
			}
			// uses "decode from calldata"
			function test2_dynamic() public returns (uint) {
				var x = new function() external returns (uint)[](3);
				x[0] = this.f1;
				x[1] = this.f2;
				x[2] = this.f3;
				return this.g(x, 0);
			}
			function test2_static() public returns (uint) {
				return this.h([this.f1, this.f2, this.f3], 0);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("test1_static()"), encodeArgs(3));
		ABI_CHECK(callContractFunction("test1_dynamic()"), encodeArgs(3));
		ABI_CHECK(callContractFunction("test2_static()"), encodeArgs(1));
		ABI_CHECK(callContractFunction("test2_dynamic()"), encodeArgs(1));
	)
}

BOOST_AUTO_TEST_CASE(decode_from_memory_complex)
{
	string sourceCode = R"(
		contract C {
			uint public _a;
			uint[] public _b;
			bytes[2] public _c;
			function C(uint a, uint[] b, bytes[2] c) {
				_a = a;
				_b = b;
				_c = c;
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C", encodeArgs(
			7, 
			// b
			encodeRefArgs(1, 3, bigint(0x21), bigint(0x22), bigint(0x23)),
			// c
			encodeRefArgs(
			encodeDyn(string("abcdefgh")),
			encodeDyn(string("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ")))
		));
		ABI_CHECK(callContractFunction("_a()"), encodeArgs(7));
		ABI_CHECK(callContractFunction("_b(uint)", 0), encodeArgs(0x21));
		ABI_CHECK(callContractFunction("_b(uint)", 1), encodeArgs(0x22));
		ABI_CHECK(callContractFunction("_b(uint)", 2), encodeArgs(0x23));
		ABI_CHECK(callContractFunction("_b(uint)", 3), encodeArgs());
		ABI_CHECK(callContractFunction("_c(uint)", 0), encodeArgs(encodeDyn(string("abcdefgh"))));

	)
}

BOOST_AUTO_TEST_CASE(short_input_value_type)
{
	string sourceCode = R"(
		contract C {
			function f(uint a, uint b) public pure returns (uint) { return a; }
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction("f(uint,uint)", 1, 2), encodeArgs(1));
		ABI_CHECK(callContractFunctionNoEncoding("f(uint,uint)", std::vector<bytes>(2, bytes(32, 0))), encodeArgs(0));
		ABI_CHECK(callContractFunctionNoEncoding("f(uint,uint)", std::vector<bytes>{bytes(32, 0), bytes(31, 0)}), encodeArgs(0));
	)
}

BOOST_AUTO_TEST_CASE(short_input_array)
{
	string sourceCode = R"(
		contract C {
			function f(uint[] a) public pure returns (uint) { return 7; }
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunctionNoEncoding("f(uint[])", encodeArgs(encodeRefArgs(1, 0))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(uint[])", encodeArgs(encodeRefArgs(1, 1))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(uint[])", encodeArgs(encodeRefArgs(1, 1, uint64_t(1)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(uint[])", encodeArgs(encodeRefArgs(1, 1, bigint(0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(uint[])", encodeArgs(encodeRefArgs(1, 2, bigint(5), bigint(6)))), encodeArgs(7));
	)
}

BOOST_AUTO_TEST_CASE(short_dynamic_input_array)
{
	string sourceCode = R"(
		contract C {
			function f(bytes[1] a) public pure returns (uint) { return 7; }
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunctionNoEncoding("f(bytes[1])", encodeArgs(encodeRefArgs())), encodeArgs(7));
	)
}

BOOST_AUTO_TEST_CASE(short_input_bytes)
{
	string sourceCode = R"(
		contract C {
			function e(bytes a) public pure returns (uint) { return 7; }
			function f(bytes[] a) public pure returns (uint) { return 7; }
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunctionNoEncoding("e(bytes)", encodeArgs(encodeRefArgs(7, bytes(5, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("e(bytes)", encodeArgs(encodeRefArgs(7, bytes(6, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("e(bytes)", encodeArgs(encodeRefArgs(7, bytes(7, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("e(bytes)", encodeArgs(encodeRefArgs(7, bytes(8, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(bytes[])", encodeArgs(encodeRefArgs(1, 7, bytes(5, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(bytes[])", encodeArgs(encodeRefArgs(1, 7, bytes(6, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(bytes[])", encodeArgs(encodeRefArgs(1, 7, bytes(7, 0)))), encodeArgs(7));
		ABI_CHECK(callContractFunctionNoEncoding("f(bytes[])", encodeArgs(encodeRefArgs(1, 7, bytes(8, 0)))), encodeArgs(7));
	)
}

BOOST_AUTO_TEST_CASE(cleanup_int_inside_arrays)
{
	string sourceCode = R"(
		contract C {
			enum E { A, B }
			function f(uint16[] a) public pure returns (uint r) { r = a[0]; }
			function g(int16[] a) public pure returns (uint256 r) { r = uint256(a[0]); }
			function h(E[] a) public pure returns (uint r) { r = uint(a[0]); }
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode);
		ABI_CHECK(callContractFunction("f(uint16[])", encodeRefArgs(1, 1, 7)), encodeArgs(7));
		ABI_CHECK(callContractFunction("g(int16[])", encodeRefArgs(1, 1, 7)), encodeArgs(7));
		ABI_CHECK(callContractFunction("f(uint16[])", encodeRefArgs(1, 1, int16_t(0xffff), 0)), encodeArgs(u256("0xffff")));
		ABI_CHECK(callContractFunction("g(int16[])", encodeRefArgs(1, 1, int16_t(0xffff), 0)), encodeArgs(u256(-1)));
		ABI_CHECK(callContractFunction("h(uint8[])", encodeRefArgs(1, 1, 0)), encodeArgs(u256(0)));
		ABI_CHECK(callContractFunction("h(uint8[])", encodeRefArgs(1, 1, 1)), encodeArgs(u256(1)));
		ABI_CHECK(callContractFunction("h(uint8[])", encodeRefArgs(1, 1, 2)), encodeArgs());
	)
}

BOOST_AUTO_TEST_CASE(storage_ptr)
{
	string sourceCode = R"(
		library L {
			struct S { uint x; uint y; }
			function f(uint[] storage r, S storage s) public returns (uint, uint, uint, uint) {
				r[2] = 8;
				s.x = 7;
				return (r[0], r[1], s.x, s.y);
			}
		}
		contract C {
			uint8 x = 3;
			L.S s;
			uint[] r;
			function f() public returns (uint, uint, uint, uint, uint, uint) {
				r.length = 6;
				r[0] = 1;
				r[1] = 2;
				r[2] = 3;
				s.x = 11;
				s.y = 12;
				var (a, b, c, d) = L.f(r, s);
				return (r[2], s.x, a, b, c, d);
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "L");
		compileAndRun(sourceCode, 0, "C", std::vector<bytes>(), map<string, Address>{{"L", m_contractAddress}});
		ABI_CHECK(callContractFunction("f()"), encodeArgs(8, 7, 1, 2, 7, 12));
	)
}

BOOST_AUTO_TEST_CASE(struct_simple)
{
	string sourceCode = R"(
		contract C {
			struct S { uint a; uint8 b; uint8 c; bytes2 d; }
			function f(S s) public pure returns (uint a, uint b, uint c, uint d) {
				a = s.a;
				b = s.b;
				c = s.c;
				d = uint(s.d);
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f((uint,uint8,uint8,bytes2))", encodeRefArgs(bigint(1), 2, 3, "ab")), encodeArgs(1, 2, 3, 'a' * 0x100 + 'b'));
	)
}

BOOST_AUTO_TEST_CASE(struct_cleanup)
{
	string sourceCode = R"(
		contract C {
			struct S { int16 a; uint8 b; bytes2 c; }
			function f(S s) public pure returns (uint256 a, uint b, uint c) {
				a = uint256(s.a);
				b = s.b;
				c = uint256(s.c);
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(
			callContractFunction("f((int16,uint8,bytes2))", encodeRefArgs(int16_t(0xf010), 0x02, "ab")),
			encodeArgs(u256("0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff010"), 2, "ab")
		);
	)
}

BOOST_AUTO_TEST_CASE(struct_short)
{
	string sourceCode = R"(
		contract C {
			struct S { int a; uint b; bytes16 c; }
			function f(S s) public pure returns (S q) {
				q = s;
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(
			callContractFunction("f((int,uint,bytes16))", vector<bytes>(1, encodeRefArgs(bigint(0xff010), bigint(0xff0002), "abcd" + string(12, 0)))),
			encodeArgs(encodeRefArgs(bigint(0xff010), bigint(0xff0002), "abcd" + string(12, 0)))
		);
		ABI_CHECK(
			callContractFunctionNoEncoding("f((int,uint,bytes16))", vector<bytes>(1, encodeRefArgs(bigint(0xff010), bigint(0xff0002), bytes(16, 0)))),
			encodeArgs(encodeRefArgs(bigint(0xff010), bigint(0xff0002)))
		);
		ABI_CHECK(
			callContractFunctionNoEncoding("f((int,uint,bytes16))", vector<bytes>(1, encodeRefArgs(bigint(0xff010), bigint(0xff0002), bytes(15, 0)))),
			encodeArgs(encodeRefArgs(bigint(0xff010), bigint(0xff0002)))
		);
	)
}

BOOST_AUTO_TEST_CASE(struct_function)
{
	string sourceCode = R"(
		contract C {
			struct S { function () external returns (uint) f; uint b; }
			function f(S s) public returns (uint, uint) {
				return (s.f(), s.b);
			}
			function test() public returns (uint, uint) {
				return this.f(S(this.g, 3));
			}
			function g() public returns (uint) { return 7; }
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("test()"), encodeArgs(7, 3));
	)
}

BOOST_AUTO_TEST_CASE(empty_struct)
{
	string sourceCode = R"(
		contract C {
			struct S { }
			function f(uint a, S s, uint b) public pure returns (uint x, uint y) {
				x = a; y = b;
			}
			function g() public returns (uint, uint) {
				return this.f(7, S(), 8);
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f(uint,(),uint)", 7, encodeRefArgs(), 8), encodeArgs(7, 8));
		ABI_CHECK(callContractFunction("g()"), encodeArgs(7, 8));
	)
}

BOOST_AUTO_TEST_CASE(mediocre_struct)
{
	string sourceCode = R"(
		contract C {
			struct S { C c; }
			function f(uint a, S[2] s1, uint b) public returns (uint r1, C r2, uint r3) {
				r1 = a;
				r2 = s1[0].c;
				r3 = b;
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		string sig = "f(uint,(address)[2],uint)";
		ABI_CHECK(callContractFunction(sig,
			7, encodeRefArgs(u160(m_contractAddress), u160(0)), 8
		), encodeArgs(7, u160(m_contractAddress), 8));
	)
}

BOOST_AUTO_TEST_CASE(mediocre2_struct)
{
	string sourceCode = R"(
		contract C {
			struct S { C c; uint[] x; }
			function f(uint a, S[2] s1, uint b) public returns (uint r1, C r2, uint r3) {
				r1 = a;
				r2 = s1[0].c;
				r3 = b;
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		string sig = "f(uint,(address,uint[])[2],uint)";
		ABI_CHECK(callContractFunction(sig,
			7, encodeRefArgs(
			u160(m_contractAddress), 
			1, 2, bigint(0x11), bigint(0x12),
			u160(0x99), 
			1, 4, bigint(0x31), bigint(0x32), bigint(0x34), bigint(0x35)),
                        8
		), encodeArgs(7, u160(m_contractAddress), 8));
	)
}

BOOST_AUTO_TEST_CASE(complex_struct)
{
	string sourceCode = R"(
		contract C {
			enum E {A, B, C}
			struct T { uint x; E e; uint8 y; }
			struct S { C c; T[] t;}
			function f(uint a, S[2] s1, S[] s2, uint b) public returns
					(uint r1, C r2, uint r3, uint r4, C r5, uint r6, E r7, uint8 r8) {
				r1 = a;
				r2 = s1[0].c;
				r3 = b;
				r4 = s2.length;
				r5 = s2[1].c;
				r6 = s2[1].t.length;
				r7 = s2[1].t[1].e;
				r8 = s2[1].t[1].y;
			}
		}
	)";
	NEW_ENCODER(
		compileAndRun(sourceCode, 0, "C");
		string sig = "f(uint,(address,(uint,uint8,uint8)[])[2],(address,(uint,uint8,uint8)[])[],uint)";
		std::vector<bytes> args = encodeArgs(
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
			1, 0), encodeRefArgs(
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
			bigint(0), 0, 0), 8
		);
		ABI_CHECK(callContractFunctionNoEncoding(sig, args), encodeArgs(7, u256(u160(m_contractAddress)), 8, 2, 0x1234, 3, 2, 0x22));
		// invalid enum value
		args[2][12] = 3;
		ABI_CHECK(callContractFunctionNoEncoding(sig, args), encodeArgs());
	)
}


BOOST_AUTO_TEST_CASE(return_dynamic_types_cross_call_simple)
{
	if (m_evmVersion == EVMVersion::homestead())
		return;

	string sourceCode = R"(
		contract C {
			function dyn() public returns (bytes) {
				return "1234567890123456789012345678901234567890";
			}
			function f() public returns (bytes) {
				return this.dyn();
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f()"), encodeArgs(encodeDyn(string("1234567890123456789012345678901234567890"))));
	)
}

BOOST_AUTO_TEST_CASE(return_dynamic_types_cross_call_advanced)
{
	if (m_evmVersion == EVMVersion::homestead())
		return;

	string sourceCode = R"(
		contract C {
			function dyn() public returns (bytes a, uint256 b, bytes20[] c, uint d) {
				a = "1234567890123456789012345678901234567890";
				b = uint256(-1);
				c = new bytes20[](4);
				c[0] = bytes20(1234);
				c[3] = bytes20(6789);
				d = 0x1234;
			}
			function f() public returns (bytes, uint256, bytes20[], uint) {
				return this.dyn();
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		ABI_CHECK(callContractFunction("f()"), encodeArgs(
			encodeDyn(string("1234567890123456789012345678901234567890")), u256(-1), encodeRefArray(
			{u256(1234), 0, 0, u256(6789)}, 4, 20),
                        0x1234
		));
	)
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(return_dynamic_types_cross_call_out_of_range, 1)
BOOST_AUTO_TEST_CASE(return_dynamic_types_cross_call_out_of_range)
{
	string sourceCode = R"(
		contract C {
			function dyn(uint x) public returns (bytes a) {
				assembly {
					mstore(0, 0x20)
					mstore(0x20, 0x21)
					return(0, x)
				}
			}
			function f(uint x) public returns (bool) {
				this.dyn(x);
				return true;
			}
		}
	)";
	BOTH_ENCODERS(
		compileAndRun(sourceCode, 0, "C");
		if (m_evmVersion == EVMVersion::homestead())
		{
			ABI_CHECK(callContractFunction("f(uint256)", 0x60), encodeArgs(true));
			ABI_CHECK(callContractFunction("f(uint256)", 0x7f), encodeArgs(true));
		}
		else
		{
			ABI_CHECK(callContractFunction("f(uint256)", 0x60), encodeArgs());
			ABI_CHECK(callContractFunction("f(uint256)", 0x61), encodeArgs(true));
		}
		ABI_CHECK(callContractFunction("f(uint256)", 0x80), encodeArgs(true));
	)
}

BOOST_AUTO_TEST_SUITE_END()

}
}
} // end namespaces
