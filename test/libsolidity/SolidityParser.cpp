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
 * Unit tests for the solidity parser.
 */

#include <string>
#include <memory>
#include <liblangutil/Scanner.h>
#include <libsolidity/parsing/Parser.h>
#include <liblangutil/ErrorReporter.h>
#include <test/Common.h>
#include <test/libsolidity/ErrorCheck.h>
#include <libsolidity/ast/ASTVisitor.h>

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace solidity::langutil;

namespace solidity::frontend::test
{

namespace
{
ASTPointer<ContractDefinition> parseText(std::string const& _source, ErrorList& _errors, bool errorRecovery = false)
{
	ErrorReporter errorReporter(_errors);
	ASTPointer<SourceUnit> sourceUnit = Parser(
		errorReporter,
		solidity::test::CommonOptions::get().evmVersion(),
		errorRecovery
	).parse(std::make_shared<Scanner>(CharStream(_source, "")));
	if (!sourceUnit)
		return ASTPointer<ContractDefinition>();
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ASTPointer<ContractDefinition> contract = dynamic_pointer_cast<ContractDefinition>(node))
			return contract;
	BOOST_FAIL("No contract found in source.");
	return ASTPointer<ContractDefinition>();
}

bool successParse(std::string const& _source)
{
	ErrorList errors;
	try
	{
		auto sourceUnit = parseText(_source, errors);
		if (!sourceUnit)
			return false;
	}
	catch (FatalError const& /*_exception*/)
	{
		if (Error::containsErrorOfType(errors, Error::Type::ParserError))
			return false;
	}
	if (Error::containsErrorOfType(errors, Error::Type::ParserError))
		return false;

	BOOST_CHECK(Error::containsOnlyWarnings(errors));
	return true;
}

Error getError(std::string const& _source, bool errorRecovery = false)
{
	ErrorList errors;
	try
	{
		parseText(_source, errors, errorRecovery);
	}
	catch (FatalError const& /*_exception*/)
	{
		// no-op
	}
	Error const* error = Error::containsErrorOfType(errors, Error::Type::ParserError);
	BOOST_REQUIRE(error);
	return *error;
}

void checkFunctionNatspec(
	FunctionDefinition const* _function,
	std::string const& _expectedDoc
)
{
	auto doc = _function->documentation()->text();
	BOOST_CHECK_MESSAGE(doc != nullptr, "Function does not have Natspec Doc as expected");
	BOOST_CHECK_EQUAL(*doc, _expectedDoc);
}

}

#define CHECK_PARSE_ERROR(source, substring) \
do \
{\
	Error err = getError((source)); \
	BOOST_CHECK(searchErrorMessage(err, (substring))); \
}\
while(0)


BOOST_AUTO_TEST_SUITE(SolidityParser)

BOOST_AUTO_TEST_CASE(reserved_keywords)
{
	BOOST_CHECK(!TokenTraits::isReservedKeyword(Token::Identifier));
	BOOST_CHECK(TokenTraits::isReservedKeyword(Token::After));
	BOOST_CHECK(!TokenTraits::isReservedKeyword(Token::Unchecked));
	BOOST_CHECK(TokenTraits::isReservedKeyword(Token::Var));
	BOOST_CHECK(TokenTraits::isReservedKeyword(Token::Reference));
	BOOST_CHECK(!TokenTraits::isReservedKeyword(Token::Illegal));
}

BOOST_AUTO_TEST_CASE(function_natspec_documentation)
{
	char const* text = R"(
		contract test {
			uint256 stateVar;
			/// This is a test function
			function functionName(bytes32 input) public returns (bytes32 out) {}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	FunctionDefinition const* function = nullptr;
	auto functions = contract->definedFunctions();

	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	checkFunctionNatspec(function, "This is a test function");
}

BOOST_AUTO_TEST_CASE(function_normal_comments)
{
	FunctionDefinition const* function = nullptr;
	char const* text = R"(
		contract test {
			uint256 stateVar;
			// We won't see this comment
			function functionName(bytes32 input) public returns (bytes32 out) {}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	auto functions = contract->definedFunctions();
	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	BOOST_CHECK_MESSAGE(function->documentation() == nullptr,
						"Should not have gotten a Natspecc comment for this function");
}

BOOST_AUTO_TEST_CASE(multiple_functions_natspec_documentation)
{
	FunctionDefinition const* function = nullptr;
	char const* text = R"(
		contract test {
			uint256 stateVar;
			/// This is test function 1
			function functionName1(bytes32 input) public returns (bytes32 out) {}
			/// This is test function 2
			function functionName2(bytes32 input) public returns (bytes32 out) {}
			// nothing to see here
			function functionName3(bytes32 input) public returns (bytes32 out) {}
			/// This is test function 4
			function functionName4(bytes32 input) public returns (bytes32 out) {}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	auto functions = contract->definedFunctions();

	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	checkFunctionNatspec(function, "This is test function 1");

	BOOST_REQUIRE_MESSAGE(function = functions.at(1), "Failed to retrieve function");
	checkFunctionNatspec(function, "This is test function 2");

	BOOST_REQUIRE_MESSAGE(function = functions.at(2), "Failed to retrieve function");
	BOOST_CHECK_MESSAGE(function->documentation() == nullptr,
						"Should not have gotten natspec comment for functionName3()");

	BOOST_REQUIRE_MESSAGE(function = functions.at(3), "Failed to retrieve function");
	checkFunctionNatspec(function, "This is test function 4");
}

BOOST_AUTO_TEST_CASE(multiline_function_documentation)
{
	FunctionDefinition const* function = nullptr;
	char const* text = R"(
		contract test {
			uint256 stateVar;
			/// This is a test function
			/// and it has 2 lines
			function functionName1(bytes32 input) public returns (bytes32 out) {}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	auto functions = contract->definedFunctions();
	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	checkFunctionNatspec(function, "This is a test function\n"
						 " and it has 2 lines");
}

BOOST_AUTO_TEST_CASE(natspec_comment_in_function_body)
{
	FunctionDefinition const* function = nullptr;
	char const* text = R"(
		contract test {
			/// fun1 description
			function fun1(uint256 a) {
				uint b;
				// I should not interfere with actual natspec comments (natspec comments on local variables not allowed anymore)
				uint256 c;
				mapping(address=>bytes32) d;
				bytes7 name = "Solidity";
			}
			/// This is a test function
			/// and it has 2 lines
			function fun(bytes32 input) public returns (bytes32 out) {}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	auto functions = contract->definedFunctions();

	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	checkFunctionNatspec(function, "fun1 description");

	BOOST_REQUIRE_MESSAGE(function = functions.at(1), "Failed to retrieve function");
	checkFunctionNatspec(function, "This is a test function\n"
						 " and it has 2 lines");
}

BOOST_AUTO_TEST_CASE(natspec_docstring_between_keyword_and_signature)
{
	FunctionDefinition const* function = nullptr;
	char const* text = R"(
		contract test {
			uint256 stateVar;
			function ///I am in the wrong place
			fun1(uint256 a) {
				uint b;
				// I should not interfere with actual natspec comments (natspec comments on local variables not allowed anymore)
				uint256 c;
				mapping(address=>bytes32) d;
				bytes7 name = "Solidity";
			}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	auto functions = contract->definedFunctions();

	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	BOOST_CHECK_MESSAGE(!function->documentation(),
						"Shouldn't get natspec docstring for this function");
}

BOOST_AUTO_TEST_CASE(natspec_docstring_after_signature)
{
	FunctionDefinition const* function = nullptr;
	char const* text = R"(
		contract test {
			uint256 stateVar;
			function fun1(uint256 a) {
				// I should have been above the function signature (natspec comments on local variables not allowed anymore)
				uint b;
				// I should not interfere with actual natspec comments (natspec comments on local variables not allowed anymore)
				uint256 c;
				mapping(address=>bytes32) d;
				bytes7 name = "Solidity";
			}
		}
	)";
	BOOST_CHECK(successParse(text));
	ErrorList errors;
	ASTPointer<ContractDefinition> contract = parseText(text, errors);
	auto functions = contract->definedFunctions();

	BOOST_REQUIRE_MESSAGE(function = functions.at(0), "Failed to retrieve function");
	BOOST_CHECK_MESSAGE(!function->documentation(),
						"Shouldn't get natspec docstring for this function");
}

BOOST_AUTO_TEST_CASE(variable_definition)
{
	char const* text = R"(
		contract test {
			function fun(uint256 a) {
				uint b;
				uint256 c;
				mapping(address=>bytes32) d;
				customtype varname;
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(variable_definition_with_initialization)
{
	char const* text = R"(
		contract test {
			function fun(uint256 a) {
				uint b = 2;
				uint256 c = 0x87;
				mapping(address=>bytes32) d;
				bytes7 name = "Solidity";
				customtype varname;
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(operator_expression)
{
	char const* text = R"(
		contract test {
			function fun(uint256 a) {
				uint256 x = (1 + 4) || false && (1 - 12) + -9;
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(complex_expression)
{
	char const* text = R"(
		contract test {
			function fun(uint256 a) {
				uint256 x = (1 + 4).member(++67)[a/=9] || true;
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(statement_starting_with_type_conversion)
{
	char const* text = R"(
		contract test {
			function fun() {
				uint64(2);
				uint64[7](3);
				uint64[](3);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(type_conversion_to_dynamic_array)
{
	char const* text = R"(
		contract test {
			function fun() {
				uint x = uint64[](3);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(import_directive)
{
	char const* text = R"(
		import "abc";
		contract test {
			function fun() {
				uint64(2);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(multiple_contracts)
{
	char const* text = R"(
		contract test {
			function fun() {
				uint64(2);
			}
		}
		contract test2 {
			function fun() {
				uint64(2);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(multiple_contracts_and_imports)
{
	char const* text = R"(
		import "abc";
		contract test {
			function fun() {
				uint64(2);
			}
		}
		import "def";
		contract test2 {
			function fun() {
				uint64(2);
			}
		}
		import "ghi";
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(contract_inheritance)
{
	char const* text = R"(
		contract base {
			function fun() {
				uint64(2);
			}
		}
		contract derived is base {
			function fun() {
				uint64(2);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(contract_multiple_inheritance)
{
	char const* text = R"(
		contract base {
			function fun() {
				uint64(2);
			}
		}
		contract derived is base, nonExisting {
			function fun() {
				uint64(2);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(contract_multiple_inheritance_with_arguments)
{
	char const* text = R"(
		contract base {
			function fun() {
				uint64(2);
			}
		}
		contract derived is base(2), nonExisting("abc", "def", base.fun()) {
			function fun() {
				uint64(2);
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(keyword_is_reserved)
{
	auto keywords = {
		"after",
		"alias",
		"apply",
		"auto",
		"byte",
		"case",
		"copyof",
		"default",
		"define",
		"final",
		"implements",
		"in",
		"inline",
		"let",
		"macro",
		"match",
		"mutable",
		"null",
		"of",
		"partial",
		"promise",
		"reference",
		"relocatable",
		"sealed",
		"sizeof",
		"static",
		"supports",
		"switch",
		"typedef",
		"typeof",
		"var"
	};

	BOOST_CHECK_EQUAL(std::size(keywords), static_cast<int>(Token::Var) - static_cast<int>(Token::After) + 1);

	for (auto const& keyword: keywords)
	{
		auto text = std::string("contract ") + keyword + " {}";
		CHECK_PARSE_ERROR(text.c_str(), string("Expected identifier but got reserved keyword '") + keyword + "'");
	}
}

BOOST_AUTO_TEST_CASE(member_access_parser_ambiguity)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; uint[][][] c; }
			function f() {
				C.S x;
				C.S memory y;
				C.S[10] memory z;
				C.S[10](x);
				x.a = 2;
				x.c[1][2][3] = 9;
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(using_for)
{
	char const* text = R"(
		contract C {
			struct s { uint a; }
			using LibraryName for uint;
			using Library2 for *;
			using Lib for s;
			function f() {
			}
		}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(complex_import)
{
	char const* text = R"(
		import "abc" as x;
		import * as x from "abc";
		import {a as b, c as d, f} from "def";
		contract x {}
	)";
	BOOST_CHECK(successParse(text));
}

BOOST_AUTO_TEST_CASE(inline_asm_end_location)
{
	auto sourceCode = std::string(R"(
	contract C {
		function f() public pure returns (uint y) {
			uint a;
			assembly { a := 0x12345678 }
			uint z = a;
			y = z;
		}
	}
	)");
	ErrorList errors;
	auto contract = parseText(sourceCode, errors);

	class CheckInlineAsmLocation: public ASTConstVisitor
	{
	public:
		bool visited = false;
		bool visit(InlineAssembly const& _inlineAsm) override
		{
			auto loc = _inlineAsm.location();
			auto asmStr = loc.source->source().substr(static_cast<size_t>(loc.start), static_cast<size_t>(loc.end - loc.start));
			BOOST_CHECK_EQUAL(asmStr, "assembly { a := 0x12345678 }");
			visited = true;

			return false;
		}
	};

	CheckInlineAsmLocation visitor;
	contract->accept(visitor);

	BOOST_CHECK_MESSAGE(visitor.visited, "No inline asm block found?!");
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
