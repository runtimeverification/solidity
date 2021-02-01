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
 * Unit tests for the view and pure checker.
 */

#include <test/libsolidity/AnalysisFramework.h>

#include <test/Common.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <tuple>

using namespace std;
using namespace solidity::langutil;

namespace solidity::frontend::test
{

BOOST_FIXTURE_TEST_SUITE(ViewPureChecker, AnalysisFramework)

BOOST_AUTO_TEST_CASE(environment_access)
{
	vector<string> view{
		"block.coinbase",
		"block.timestamp",
		"block.difficulty",
		"block.number",
		"block.gaslimit",
		"blockhash(7)",
		"gasleft()",
		"msg.value",
		"msg.sender",
		"tx.origin",
		"tx.gasprice",
		"this",
		"address(1).balance",
<<<<<<< ours
		"address(1).codesize"
=======
>>>>>>> theirs
	};
	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
		view.emplace_back("address(0x4242).staticcall(\"\")");

	// ``block.blockhash`` and ``blockhash`` are tested separately below because their usage will
	// produce warnings that can't be handled in a generic way.
	vector<string> pure{
	//	"msg.data",
	//	"msg.data[0]",
	//	"msg.sig",
		"msg",
		"block",
		"tx"
	};
	for (string const& x: view)
	{
		CHECK_ERROR(
			"contract C { function f() pure public { " + x + "; } }",
			TypeError,
			"Function declared as pure, but this expression (potentially) reads from the environment or state and thus requires \"view\""
		);
	}
	for (string const& x: pure)
	{
		CHECK_WARNING(
			"contract C { function f() view public { " + x + "; } }",
			"Function state mutability can be restricted to pure"
		);
	}

	CHECK_WARNING_ALLOW_MULTI(
		"contract C { function f() view public { blockhash; } }",
		(std::vector<std::string>{
			"Function state mutability can be restricted to pure",
			"Statement has no effect."
	}));

	CHECK_ERROR(
		"contract C { function f() view public { block.blockhash; } }",
		TypeError,
		"\"block.blockhash()\" has been deprecated in favor of \"blockhash()\""
	);
<<<<<<< ours

}

BOOST_AUTO_TEST_CASE(modifiers)
{
	string text = R"(
		contract D {
			uint x;
			modifier purem(uint) { _; }
			modifier viewm(uint) { uint a = x; _; a; }
			modifier nonpayablem(uint) { x = 2; _; }
		}
		contract C is D {
			function f() purem(0) pure public {}
			function g() viewm(0) view public {}
			function h() nonpayablem(0) public {}
			function i() purem(x) view public {}
			function j() viewm(x) view public {}
			function k() nonpayablem(x) public {}
			function l() purem(x = 2) public {}
			function m() viewm(x = 2) public {}
			function n() nonpayablem(x = 2) public {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(interface)
{
	string text = R"(
		interface D {
			function f() view external;
		}
		contract C is D {
			function f() view external {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(overriding)
{
	string text = R"(
		contract D {
			uint x;
			function f() public { x = 2; }
		}
		contract C is D {
			function f() public {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(returning_structs)
{
	string text = R"(
		contract C {
			struct S { uint x; }
			S s;
			function f() view internal returns (S storage) {
				return s;
			}
			function g() public {
				f().x = 2;
			}
			function h() view public {
				f();
				f().x;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(mappings)
{
	string text = R"(
		contract C {
			mapping(uint => uint) a;
			function f() view public {
				a;
			}
			function g() view public {
				a[2];
			}
			function h() public {
				a[2] = 3;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(local_storage_variables)
{
	string text = R"(
		contract C {
			struct S { uint a; }
			S s;
			function f() view public {
				S storage x = s;
				x;
			}
			function g() view public {
				S storage x = s;
				x = s;
			}
			function i() public {
				s.a = 2;
			}
			function h() public {
				S storage x = s;
				x.a = 2;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(builtin_functions)
{
	string text = R"(
		contract C {
			function f() public {
				address(this).transfer(1);
				require(address(this).send(2));
				selfdestruct(address(this));
			//	require(address(this).delegatecall());
			//	require(address(this).call());
			}
			function g() pure public {
				bytes32 x = keccak256("abc");
				bytes32 y = sha256("abc");
				address z = ecrecover(1, 2, 3, 4);
				uint256 val = 0;
				uint256[2] memory a = ecadd([val, val], [val, val]);
				uint256[2] memory b = ecmul([val, val], val);
				uint256[2][] memory p1;
				uint256[4][] memory p2;
				bool c = ecpairing(p1, p2);
				require(true);
				assert(true);
				x; y; z; a; b; c;
			}
			function() payable public {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(function_types)
{
	string text = R"(
		contract C {
			function f() pure public {
				function () external nonpayFun;
				function () external view viewFun;
				function () external pure pureFun;

				nonpayFun;
				viewFun;
				pureFun;
				pureFun();
			}
			function g() view public {
				function () external view viewFun;

				viewFun();
			}
			function h() public {
				function () external nonpayFun;

				nonpayFun();
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(selector)
{
	string text = R"(
		contract C {
			uint public x;
			function f() payable public {
			}
			function g() pure public returns (bytes4) {
				return this.f.selector ^ this.x.selector;
			}
		}
	)";
	std::vector<string> msgs{"Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md", "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md"};
	CHECK_ERROR_ALLOW_MULTI(text, TypeError, msgs);
}

BOOST_AUTO_TEST_CASE(selector_complex)
{
	string text = R"(
		contract C {
			function f(C c) pure public returns (C) {
				return c;
			}
			function g() pure public returns (bytes4) {
				// By passing `this`, we read from the state, even if f itself is pure.
				return f(this).f.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
=======
>>>>>>> theirs
}

BOOST_AUTO_TEST_CASE(address_staticcall)
{
	string text = R"(
		contract C {
			function i() view public returns (bool) {
				(bool success,) = address(0x4242).staticcall("");
				return success;
			}
		}
	)";
<<<<<<< ours
	CHECK_ERROR(text, TypeError, "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}

BOOST_AUTO_TEST_CASE(creation)
{
	string text = R"(
		contract D {}
		contract C {
			function f() public { new D(); }
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(assembly)
{
	string text = R"(
		contract C {
			struct S { uint x; }
			S s;
			function e() pure public {
				assembly { mstore(keccak256(0, 20), mul(s_slot, 2)) }
			}
			function f() pure public {
				uint x;
				assembly { x := 7 }
			}
			function g() view public {
				assembly { for {} 1 { pop(sload(0)) } { } pop(gas) }
			}
			function h() view public {
				assembly { function g() { pop(blockhash(20)) } }
			}
			function j() public {
				assembly { pop(call(0, 1, 2, 3, 4, 5, 6)) }
			}
			function k() public {
				assembly { pop(call(gas, 1, 2, 3, 4, 5, 6)) }
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	std::vector<std::string> msgs{msg,msg,msg,msg,msg,msg};
	CHECK_ERROR_ALLOW_MULTI(text, SyntaxError, msgs);
}
=======
	if (!solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
		CHECK_ERROR(text, TypeError, "\"staticcall\" is not supported by the VM version.");
	else
		CHECK_SUCCESS_NO_WARNINGS(text);
}

>>>>>>> theirs

BOOST_AUTO_TEST_CASE(assembly_staticcall)
{
	string text = R"(
		contract C {
			function i() view public {
				assembly { pop(staticcall(gas(), 1, 2, 3, 4, 5)) }
			}
		}
	)";
<<<<<<< ours
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(assembly_jump)
{
	string text = R"(
		contract C {
			function k() public {
				assembly { jump(2) }
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(constant)
{
	string text = R"(
		contract C {
			uint constant x = 2;
			function k() pure public returns (uint) {
				return x;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

=======
	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
		CHECK_SUCCESS_NO_WARNINGS(text);
}

>>>>>>> theirs
BOOST_AUTO_TEST_SUITE_END()

}
