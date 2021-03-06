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
 * Unit tests for the name and type resolution of the solidity parser.
 */

#include <test/libsolidity/AnalysisFramework.h>

#include <test/Common.h>

#include <libsolidity/ast/AST.h>

#include <libsolutil/Keccak256.h>

#include <boost/test/unit_test.hpp>

#include <string>

using namespace std;
using namespace solidity::langutil;

namespace solidity::frontend::test
{

BOOST_FIXTURE_TEST_SUITE(SolidityNameAndTypeResolution, AnalysisFramework)

BOOST_AUTO_TEST_CASE(function_no_implementation)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		abstract contract test {
			function functionName(bytes32 input) public virtual returns (bytes32 out);
		}
	)";
	sourceUnit = parseAndAnalyse(text);
	std::vector<ASTPointer<ASTNode>> nodes = sourceUnit->nodes();
	ContractDefinition* contract = dynamic_cast<ContractDefinition*>(nodes[1].get());
	BOOST_REQUIRE(contract);
	BOOST_CHECK(!contract->annotation().unimplementedDeclarations->empty());
	BOOST_CHECK(!contract->definedFunctions()[0]->isImplemented());
}

BOOST_AUTO_TEST_CASE(abstract_contract)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		abstract contract base { function foo() public virtual; }
		contract derived is base { function foo() public override {} }
	)";
	sourceUnit = parseAndAnalyse(text);
	std::vector<ASTPointer<ASTNode>> nodes = sourceUnit->nodes();
	ContractDefinition* base = dynamic_cast<ContractDefinition*>(nodes[1].get());
	ContractDefinition* derived = dynamic_cast<ContractDefinition*>(nodes[2].get());
	BOOST_REQUIRE(base);
	BOOST_CHECK(!base->annotation().unimplementedDeclarations->empty());
	BOOST_CHECK(!base->definedFunctions()[0]->isImplemented());
	BOOST_REQUIRE(derived);
	BOOST_CHECK(derived->annotation().unimplementedDeclarations->empty());
	BOOST_CHECK(derived->definedFunctions()[0]->isImplemented());
}

BOOST_AUTO_TEST_CASE(abstract_contract_with_overload)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		abstract contract base { function foo(bool) public virtual; }
		abstract contract derived is base { function foo(uint) public {} }
	)";
	sourceUnit = parseAndAnalyse(text);
	std::vector<ASTPointer<ASTNode>> nodes = sourceUnit->nodes();
	ContractDefinition* base = dynamic_cast<ContractDefinition*>(nodes[1].get());
	ContractDefinition* derived = dynamic_cast<ContractDefinition*>(nodes[2].get());
	BOOST_REQUIRE(base);
	BOOST_CHECK(!base->annotation().unimplementedDeclarations->empty());
	BOOST_REQUIRE(derived);
	BOOST_CHECK(!derived->annotation().unimplementedDeclarations->empty());
}

BOOST_AUTO_TEST_CASE(implement_abstract_via_constructor)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		abstract contract base { function foo() public virtual; }
		abstract contract foo is base { constructor() {} }
	)";
	sourceUnit = parseAndAnalyse(text);
	std::vector<ASTPointer<ASTNode>> nodes = sourceUnit->nodes();
	BOOST_CHECK_EQUAL(nodes.size(), 3);
	ContractDefinition* derived = dynamic_cast<ContractDefinition*>(nodes[2].get());
	BOOST_REQUIRE(derived);
	BOOST_CHECK(!derived->annotation().unimplementedDeclarations->empty());
}

BOOST_AUTO_TEST_CASE(function_canonical_signature)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		contract Test {
			function foo(uint256 arg1, uint64 arg2, bool arg3) public returns (uint256 ret) {
				ret = arg1 + arg2;
			}
		}
	)";
	sourceUnit = parseAndAnalyse(text);
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			BOOST_CHECK_EQUAL("foo(uint256,uint64,bool)", functions[0]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(function_canonical_signature_no_type_aliases)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		contract Test {
			function boo(uint, bytes32, address) public returns (uint ret) {
				ret = 5;
			}
		}
	)";
	sourceUnit = parseAndAnalyse(text);
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			if (functions.empty())
				continue;
			BOOST_CHECK_EQUAL("boo(uint,bytes32,address)", functions[0]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(function_external_types)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		contract C {
			uint a;
		}
		contract Test {
			function boo(uint, bool, bytes8, bool[2] calldata, uint[] calldata, C, address[] calldata) external returns (uint ret) {
				ret = 5;
			}
		}
	)";
	sourceUnit = parseAndAnalyse(text);
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			if (functions.empty())
				continue;
			BOOST_CHECK_EQUAL("boo(uint,bool,bytes8,bool[2],uint[],address,address[])", functions[0]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(enum_external_type)
{
	SourceUnit const* sourceUnit = nullptr;
	char const* text = R"(
		// test for bug #1801
		contract Test {
			enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
			function boo(ActionChoices enumArg) external returns (uint ret) {
				ret = 5;
			}
		}
	)";
	sourceUnit = parseAndAnalyse(text);
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			if (functions.empty())
				continue;
			BOOST_CHECK_EQUAL("boo(uint8)", functions[0]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(external_struct_signatures)
{
	char const* text = R"(
		pragma abicoder v2;
		contract Test {
			enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
			struct Simple { uint i; }
			struct Nested { X[2][] a; uint y; }
			struct X { bytes32 x; Test t; Simple[] s; }
			function f(ActionChoices, uint, Simple calldata) external {}
			function g(Test, Nested calldata) external {}
			function h(function(Nested memory) external returns (uint)[] calldata) external {}
			function i(Nested[] calldata) external {}
		}
	)";
	// Ignore analysis errors. This test only checks that correct signatures
	// are generated for external structs, but they are not yet supported
	// in code generation and therefore cause an error in the TypeChecker.
	SourceUnit const* sourceUnit = parseAnalyseAndReturnError(text, false, true, true).first;
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			BOOST_REQUIRE(!functions.empty());
			BOOST_CHECK_EQUAL("f(uint8,uint,(uint))", functions[0]->externalSignature());
			BOOST_CHECK_EQUAL("g(address,((bytes32,address,(uint)[])[2][],uint))", functions[1]->externalSignature());
			BOOST_CHECK_EQUAL("h(function[])", functions[2]->externalSignature());
			BOOST_CHECK_EQUAL("i(((bytes32,address,(uint)[])[2][],uint)[])", functions[3]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(external_struct_signatures_in_libraries)
{
	char const* text = R"(
		pragma abicoder v2;
		library Test {
			enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
			struct Simple { uint i; }
			struct Nested { X[2][] a; uint y; }
			struct X { bytes32 x; Test t; Simple[] s; }
			function f(ActionChoices, uint, Simple calldata) external {}
			function g(Test, Nested calldata) external {}
			function h(function(Nested memory) external returns (uint)[] calldata) external {}
			function i(Nested[] calldata) external {}
		}
	)";
	// Ignore analysis errors. This test only checks that correct signatures
	// are generated for external structs, but calldata structs are not yet supported
	// in code generation and therefore cause an error in the TypeChecker.
	SourceUnit const* sourceUnit = parseAnalyseAndReturnError(text, false, true, true).first;
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			BOOST_REQUIRE(!functions.empty());
			BOOST_CHECK_EQUAL("f(Test.ActionChoices,uint,Test.Simple)", functions[0]->externalSignature());
			BOOST_CHECK_EQUAL("g(Test,Test.Nested)", functions[1]->externalSignature());
			BOOST_CHECK_EQUAL("h(function[])", functions[2]->externalSignature());
			BOOST_CHECK_EQUAL("i(Test.Nested[])", functions[3]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(struct_with_mapping_in_library)
{
	char const* text = R"(
		library Test {
			struct Nested { mapping(uint => uint)[2][] a; uint y; }
			struct X { Nested n; }
			function f(X storage x) external {}
		}
	)";
	SourceUnit const* sourceUnit = parseAndAnalyse(text);
	for (ASTPointer<ASTNode> const& node: sourceUnit->nodes())
		if (ContractDefinition* contract = dynamic_cast<ContractDefinition*>(node.get()))
		{
			auto functions = contract->definedFunctions();
			BOOST_REQUIRE(!functions.empty());
			BOOST_CHECK_EQUAL("f(Test.X storage)", functions[0]->externalSignature());
		}
}

BOOST_AUTO_TEST_CASE(state_variable_accessors)
{
	char const* text = R"(
		contract test {
			function fun() public {
				uint64(2);
			}
			uint256 public foo;
			mapping(uint=>bytes4) public map;
			mapping(uint=>mapping(uint=>bytes4)) public multiple_map;
		}
	)";

	SourceUnit const* source;
	ContractDefinition const* contract;
	source = parseAndAnalyse(text);
	BOOST_REQUIRE((contract = retrieveContractByName(*source, "test")) != nullptr);
	FunctionTypePointer function = retrieveFunctionBySignature(*contract, "foo()");
	BOOST_REQUIRE(function && function->hasDeclaration());
	auto returnParams = function->returnParameterTypes();
	BOOST_CHECK_EQUAL(returnParams.at(0)->canonicalName(), "uint256");
	BOOST_CHECK(function->stateMutability() == StateMutability::View);

	function = retrieveFunctionBySignature(*contract, "map(uint256)");
	BOOST_REQUIRE(function && function->hasDeclaration());
	auto params = function->parameterTypes();
	BOOST_CHECK_EQUAL(params.at(0)->canonicalName(), "uint256");
	returnParams = function->returnParameterTypes();
	BOOST_CHECK_EQUAL(returnParams.at(0)->canonicalName(), "bytes4");
	BOOST_CHECK(function->stateMutability() == StateMutability::View);

	function = retrieveFunctionBySignature(*contract, "multiple_map(uint256,uint256)");
	BOOST_REQUIRE(function && function->hasDeclaration());
	params = function->parameterTypes();
	BOOST_CHECK_EQUAL(params.at(0)->canonicalName(), "uint256");
	BOOST_CHECK_EQUAL(params.at(1)->canonicalName(), "uint256");
	returnParams = function->returnParameterTypes();
	BOOST_CHECK_EQUAL(returnParams.at(0)->canonicalName(), "bytes4");
	BOOST_CHECK(function->stateMutability() == StateMutability::View);
}

BOOST_AUTO_TEST_CASE(private_state_variable)
{
	char const* text = R"(
		contract test {
			function fun() public {
				uint64(2);
			}
			uint256 private foo;
			uint256 internal bar;
		}
	)";

	ContractDefinition const* contract;
	SourceUnit const* source = parseAndAnalyse(text);
	BOOST_CHECK((contract = retrieveContractByName(*source, "test")) != nullptr);
	FunctionTypePointer function;
	function = retrieveFunctionBySignature(*contract, "foo()");
	BOOST_CHECK_MESSAGE(function == nullptr, "Accessor function of a private variable should not exist");
	function = retrieveFunctionBySignature(*contract, "bar()");
	BOOST_CHECK_MESSAGE(function == nullptr, "Accessor function of an internal variable should not exist");
}

BOOST_AUTO_TEST_CASE(string)
{
	char const* sourceCode = R"(
		contract C {
			string s;
			function f(string calldata x) external { s = x; }
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(dynamic_return_types_not_possible)
{
<<<<<<< ours

BOOST_AUTO_TEST_CASE(array_copy_with_different_types1)
{
	char const* text = R"(
		contract c {
			bytes a;
			uint256[] b;
			function f() public { b = a; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Type bytes storage ref is not implicitly convertible to expected type uint256[] storage ref.");
	text = R"(
		contract c {
			bytes a;
			uint[] b;
			function f() public { b = a; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Type bytes storage ref is not implicitly convertible to expected type uint[] storage ref.");
}

BOOST_AUTO_TEST_CASE(array_copy_with_different_types2)
{
	char const* text = R"(
		contract c {
			uint32[] a;
			uint8[] b;
			function f() public { b = a; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Type uint32[] storage ref is not implicitly convertible to expected type uint8[] storage ref.");
}

BOOST_AUTO_TEST_CASE(array_copy_with_different_types_conversion_possible)
{
	char const* text = R"(
		contract c {
			uint32[] a;
			uint8[] b;
			function f() public { a = b; }
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(array_copy_with_different_types_static_dynamic)
{
	char const* text = R"(
		contract c {
			uint32[] a;
			uint8[80] b;
			function f() public { a = b; }
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(array_copy_with_different_types_dynamic_static)
{
	char const* text = R"(
		contract c {
			uint256[] a;
			uint256[80] b;
			function f() public { b = a; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Type uint256[] storage ref is not implicitly convertible to expected type uint256[80] storage ref.");
	text = R"(
		contract c {
			uint[] a;
			uint[80] b;
			function f() public { b = a; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Type uint[] storage ref is not implicitly convertible to expected type uint[80] storage ref.");
}

BOOST_AUTO_TEST_CASE(array_of_undeclared_type)
{
	char const* text = R"(
		contract c {
			a[] public foo;
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found or not unique.");
}

BOOST_AUTO_TEST_CASE(storage_variable_initialization_with_incorrect_type_int)
{
	char const* text = R"(
		contract c {
			uint8 a = 1000;
		}
	)";
	CHECK_ERROR(text, TypeError, "Type int_const 1000 is not implicitly convertible to expected type uint8.");
}

BOOST_AUTO_TEST_CASE(storage_variable_initialization_with_incorrect_type_string)
{
	char const* text = R"(
		contract c {
			uint256 a = "abc";
		}
	)";
	CHECK_ERROR(text, TypeError, "Type literal_string \"abc\" is not implicitly convertible to expected type uint256.");
	text = R"(
		contract c {
			uint a = "abc";
		}
	)";
	CHECK_ERROR(text, TypeError, "Type literal_string \"abc\" is not implicitly convertible to expected type uint.");
}

BOOST_AUTO_TEST_CASE(test_fromElementaryTypeName)
{

	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::Int, 0, 0)) == *make_shared<IntegerType>(IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 8, 0)) == *make_shared<IntegerType>(8, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 16, 0)) == *make_shared<IntegerType>(16, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 24, 0)) == *make_shared<IntegerType>(24, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 32, 0)) == *make_shared<IntegerType>(32, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 40, 0)) == *make_shared<IntegerType>(40, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 48, 0)) == *make_shared<IntegerType>(48, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 56, 0)) == *make_shared<IntegerType>(56, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 64, 0)) == *make_shared<IntegerType>(64, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 72, 0)) == *make_shared<IntegerType>(72, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 80, 0)) == *make_shared<IntegerType>(80, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 88, 0)) == *make_shared<IntegerType>(88, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 96, 0)) == *make_shared<IntegerType>(96, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 104, 0)) == *make_shared<IntegerType>(104, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 112, 0)) == *make_shared<IntegerType>(112, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 120, 0)) == *make_shared<IntegerType>(120, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 128, 0)) == *make_shared<IntegerType>(128, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 136, 0)) == *make_shared<IntegerType>(136, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 144, 0)) == *make_shared<IntegerType>(144, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 152, 0)) == *make_shared<IntegerType>(152, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 160, 0)) == *make_shared<IntegerType>(160, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 168, 0)) == *make_shared<IntegerType>(168, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 176, 0)) == *make_shared<IntegerType>(176, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 184, 0)) == *make_shared<IntegerType>(184, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 192, 0)) == *make_shared<IntegerType>(192, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 200, 0)) == *make_shared<IntegerType>(200, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 208, 0)) == *make_shared<IntegerType>(208, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 216, 0)) == *make_shared<IntegerType>(216, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 224, 0)) == *make_shared<IntegerType>(224, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 232, 0)) == *make_shared<IntegerType>(232, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 240, 0)) == *make_shared<IntegerType>(240, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 248, 0)) == *make_shared<IntegerType>(248, IntegerType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, 256, 0)) == *make_shared<IntegerType>(256, IntegerType::Modifier::Signed));

	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UInt, 0, 0)) == *make_shared<IntegerType>(IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 8, 0)) == *make_shared<IntegerType>(8, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 16, 0)) == *make_shared<IntegerType>(16, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 24, 0)) == *make_shared<IntegerType>(24, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 32, 0)) == *make_shared<IntegerType>(32, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 40, 0)) == *make_shared<IntegerType>(40, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 48, 0)) == *make_shared<IntegerType>(48, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 56, 0)) == *make_shared<IntegerType>(56, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 64, 0)) == *make_shared<IntegerType>(64, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 72, 0)) == *make_shared<IntegerType>(72, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 80, 0)) == *make_shared<IntegerType>(80, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 88, 0)) == *make_shared<IntegerType>(88, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 96, 0)) == *make_shared<IntegerType>(96, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 104, 0)) == *make_shared<IntegerType>(104, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 112, 0)) == *make_shared<IntegerType>(112, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 120, 0)) == *make_shared<IntegerType>(120, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 128, 0)) == *make_shared<IntegerType>(128, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 136, 0)) == *make_shared<IntegerType>(136, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 144, 0)) == *make_shared<IntegerType>(144, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 152, 0)) == *make_shared<IntegerType>(152, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 160, 0)) == *make_shared<IntegerType>(160, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 168, 0)) == *make_shared<IntegerType>(168, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 176, 0)) == *make_shared<IntegerType>(176, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 184, 0)) == *make_shared<IntegerType>(184, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 192, 0)) == *make_shared<IntegerType>(192, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 200, 0)) == *make_shared<IntegerType>(200, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 208, 0)) == *make_shared<IntegerType>(208, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 216, 0)) == *make_shared<IntegerType>(216, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 224, 0)) == *make_shared<IntegerType>(224, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 232, 0)) == *make_shared<IntegerType>(232, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 240, 0)) == *make_shared<IntegerType>(240, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 248, 0)) == *make_shared<IntegerType>(248, IntegerType::Modifier::Unsigned));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, 256, 0)) == *make_shared<IntegerType>(256, IntegerType::Modifier::Unsigned));

	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::Byte, 0, 0)) == *make_shared<FixedBytesType>(1));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 1, 0)) == *make_shared<FixedBytesType>(1));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 2, 0)) == *make_shared<FixedBytesType>(2));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 3, 0)) == *make_shared<FixedBytesType>(3));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 4, 0)) == *make_shared<FixedBytesType>(4));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 5, 0)) == *make_shared<FixedBytesType>(5));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 6, 0)) == *make_shared<FixedBytesType>(6));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 7, 0)) == *make_shared<FixedBytesType>(7));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 8, 0)) == *make_shared<FixedBytesType>(8));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 9, 0)) == *make_shared<FixedBytesType>(9));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 10, 0)) == *make_shared<FixedBytesType>(10));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 11, 0)) == *make_shared<FixedBytesType>(11));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 12, 0)) == *make_shared<FixedBytesType>(12));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 13, 0)) == *make_shared<FixedBytesType>(13));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 14, 0)) == *make_shared<FixedBytesType>(14));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 15, 0)) == *make_shared<FixedBytesType>(15));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 16, 0)) == *make_shared<FixedBytesType>(16));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 17, 0)) == *make_shared<FixedBytesType>(17));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 18, 0)) == *make_shared<FixedBytesType>(18));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 19, 0)) == *make_shared<FixedBytesType>(19));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 20, 0)) == *make_shared<FixedBytesType>(20));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 21, 0)) == *make_shared<FixedBytesType>(21));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 22, 0)) == *make_shared<FixedBytesType>(22));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 23, 0)) == *make_shared<FixedBytesType>(23));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 24, 0)) == *make_shared<FixedBytesType>(24));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 25, 0)) == *make_shared<FixedBytesType>(25));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 26, 0)) == *make_shared<FixedBytesType>(26));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 27, 0)) == *make_shared<FixedBytesType>(27));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 28, 0)) == *make_shared<FixedBytesType>(28));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 29, 0)) == *make_shared<FixedBytesType>(29));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 30, 0)) == *make_shared<FixedBytesType>(30));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 31, 0)) == *make_shared<FixedBytesType>(31));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, 32, 0)) == *make_shared<FixedBytesType>(32));

	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::Fixed, 0, 0)) == *make_shared<FixedPointType>(128, 18, FixedPointType::Modifier::Signed));
	BOOST_CHECK(*Type::fromElementaryTypeName(ElementaryTypeNameToken(Token::UFixed, 0, 0)) == *make_shared<FixedPointType>(128, 18, FixedPointType::Modifier::Unsigned));
}

BOOST_AUTO_TEST_CASE(test_byte_is_alias_of_byte1)
{
	char const* text = R"(
		contract c {
			bytes arr;
			function f() public { byte a = arr[0];}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(warns_assigning_decimal_to_bytesxx)
{
	char const* text = R"(
		contract Foo {
			bytes32 a = 7;
		}
	)";
	CHECK_WARNING(text, "Decimal literal assigned to bytesXX variable will be left-aligned.");
}

BOOST_AUTO_TEST_CASE(does_not_warn_assigning_hex_number_to_bytesxx)
{
	char const* text = R"(
		contract Foo {
			bytes32 a = 0x1234;
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(explicit_conversion_from_decimal_to_bytesxx)
{
	char const* text = R"(
		contract Foo {
			bytes32 a = bytes32(7);
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(combining_hex_and_denomination)
{
	char const* text = R"(
		contract Foo {
			uint constant x = 0x01 wei;
		}
	)";
	CHECK_WARNING(text, "Hexadecimal numbers with unit denominations are deprecated.");

	char const* textV050 = R"(
		pragma experimental "v0.5.0";

		contract Foo {
			uint constant x = 0x01 wei;
		}
	)";
	CHECK_ERROR(textV050, TypeError, "Hexadecimal numbers cannot be used with unit denominations.");
}

BOOST_AUTO_TEST_CASE(assigning_value_to_const_variable)
{
	char const* text = R"(
		contract Foo {
			function changeIt() public { x = 9; }
			uint constant x = 56;
		}
	)";
	CHECK_ERROR(text, TypeError, "Cannot assign to a constant variable.");
}

BOOST_AUTO_TEST_CASE(assigning_state_to_const_variable_0_4_x)
{
	char const* text = R"(
		contract C {
			address constant x = msg.sender;
		}
	)";
	CHECK_WARNING(text, "Initial value for constant variable has to be compile-time constant.");
}

BOOST_AUTO_TEST_CASE(assigning_state_to_const_variable)
{
	char const* text = R"(
		pragma experimental "v0.5.0";

		contract C {
			address constant x = msg.sender;
		}
	)";
	CHECK_ERROR(text, TypeError, "Initial value for constant variable has to be compile-time constant.");
}

BOOST_AUTO_TEST_CASE(constant_string_literal_disallows_assignment)
{
	char const* text = R"(
		contract Test {
			string constant x = "abefghijklmnopqabcdefghijklmnopqabcdefghijklmnopqabca";
			function f() public {
				x[0] = "f";
			}
		}
	)";

	// Even if this is made possible in the future, we should not allow assignment
	// to elements of constant arrays.
	CHECK_ERROR(text, TypeError, "Index access for string is not possible.");
}

BOOST_AUTO_TEST_CASE(assign_constant_function_value_to_constant_0_4_x)
{
	char const* text = R"(
		contract C {
			function () constant returns (uint) x;
			uint constant y = x();
		}
	)";
	CHECK_WARNING(text, "Initial value for constant variable has to be compile-time constant.");
}

BOOST_AUTO_TEST_CASE(assign_constant_function_value_to_constant)
{
	char const* text = R"(
		pragma experimental "v0.5.0";

		contract C {
			function () constant returns (uint) x;
			uint constant y = x();
		}
	)";
	CHECK_ERROR(text, TypeError, "Initial value for constant variable has to be compile-time constant.");
}

BOOST_AUTO_TEST_CASE(assignment_to_const_var_involving_conversion)
{
	char const* text = R"(
		contract C {
			C constant x = C(0x123);
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(assignment_to_const_var_involving_expression)
{
	char const* text = R"(
		contract C {
			uint constant x = 0x123 + 0x456;
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(assignment_to_const_var_involving_keccak)
{
	char const* text = R"(
		contract C {
			bytes32 constant x = keccak256("abc");
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(assignment_to_const_array_vars)
{
	char const* text = R"(
		contract C {
			uint[3] constant x = [uint(1), 2, 3];
		}
	)";
	CHECK_ERROR(text, TypeError, "implemented");
}

BOOST_AUTO_TEST_CASE(assignment_to_const_string_bytes)
{
	char const* text = R"(
		contract C {
			bytes constant a = "\x00\x01\x02";
			bytes constant b = hex"000102";
			string constant c = "hello";
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(constant_struct)
{
	char const* text = R"(
		contract C {
			struct S { uint x; uint[] y; }
			S constant x = S(5, new uint[](4));
		}
	)";
	CHECK_ERROR(text, TypeError, "implemented");
}

BOOST_AUTO_TEST_CASE(address_is_constant)
{
	char const* text = R"(
		contract C {
			address constant x = 0x1212121212121212121212121212121212121212;
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(uninitialized_const_variable)
{
	char const* text = R"(
		contract Foo {
			uint constant y;
		}
	)";
	CHECK_ERROR(text, TypeError, "Uninitialized \"constant\" variable.");
}

BOOST_AUTO_TEST_CASE(overloaded_function_cannot_resolve)
{
	char const* sourceCode = R"(
		contract test {
			function f() public returns (uint) { return 1; }
			function f(uint a) public returns (uint) { return a; }
			function g() public returns (uint) { return f(3, 5); }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "No matching declaration found after argument-dependent lookup.");
}

BOOST_AUTO_TEST_CASE(ambiguous_overloaded_function)
{
	// literal 1 can be both converted to uint and uint8, so the call is ambiguous.
	char const* sourceCode = R"(
		contract test {
			function f(uint8 a) public returns (uint) { return a; }
			function f(uint a) public returns (uint) { return 2*a; }
			function g() public returns (uint) { return f(1); }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "No unique declaration found after argument-dependent lookup.");
}

BOOST_AUTO_TEST_CASE(assignment_of_nonoverloaded_function)
{
	char const* sourceCode = R"(
		contract test {
			function f(uint a) public returns (uint) { return 2 * a; }
			function g() public returns (uint) { var x = f; return x(7); }
		}
	)";
	CHECK_SUCCESS(sourceCode);
}

BOOST_AUTO_TEST_CASE(assignment_of_overloaded_function)
{
	char const* sourceCode = R"(
		contract test {
			function f() public returns (uint) { return 1; }
			function f(uint a) public returns (uint) { return 2 * a; }
			function g() public returns (uint) { var x = f; return x(7); }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "No matching declaration found after variable lookup.");
}

BOOST_AUTO_TEST_CASE(external_types_clash)
{
	char const* sourceCode = R"(
		contract base {
			enum a { X }
			function f(a) public { }
		}
		contract test is base {
			function f(uint8 a) public { }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Function overload clash during conversion to external types for arguments.");
}

BOOST_AUTO_TEST_CASE(override_changes_return_types)
{
	char const* sourceCode = R"(
		contract base {
			function f(uint a) public returns (uint) { }
		}
		contract test is base {
			function f(uint a) public returns (uint8) { }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Overriding function return types differ");
}

BOOST_AUTO_TEST_CASE(equal_overload)
{
	char const* sourceCode = R"(
		contract C {
			function test(uint a) public returns (uint b) { }
			function test(uint a) external {}
		}
	)";
	CHECK_ALLOW_MULTI(sourceCode, (vector<pair<Error::Type, string>>{
		{Error::Type::DeclarationError, "Function with same name and arguments defined twice."},
		{Error::Type::TypeError, "Overriding function visibility differs"}
	}));
}

BOOST_AUTO_TEST_CASE(uninitialized_var)
{
	char const* sourceCode = R"(
		contract C {
			function f() public returns (uint) { var x; return 2; }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Assignment necessary for type detection.");
}

BOOST_AUTO_TEST_CASE(string)
{
	char const* sourceCode = R"(
		contract C {
			string s;
			function f(string x) external { s = x; }
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(invalid_utf8_implicit)
{
	char const* sourceCode = R"(
		contract C {
			string s = "\xa0\x00";
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "invalid UTF-8");
}

BOOST_AUTO_TEST_CASE(invalid_utf8_explicit)
{
	char const* sourceCode = R"(
		contract C {
			string s = string("\xa0\x00");
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Explicit type conversion not allowed");
}

BOOST_AUTO_TEST_CASE(large_utf8_codepoint)
{
	char const* sourceCode = R"(
		contract C {
			string s = "\xf0\x9f\xa6\x84";
		}
	)";
	CHECK_SUCCESS(sourceCode);
}

BOOST_AUTO_TEST_CASE(string_index)
{
	char const* sourceCode = R"(
		contract C {
			string s;
			function f() public { var a = s[2]; }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Index access for string is not possible.");
}

BOOST_AUTO_TEST_CASE(string_length)
{
	char const* sourceCode = R"(
		contract C {
			string s;
			function f() public { var a = s.length; }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Member \"length\" not found or not visible after argument-dependent lookup in string storage ref");
}

BOOST_AUTO_TEST_CASE(negative_integers_to_signed_out_of_bound)
{
	char const* sourceCode = R"(
		contract test {
			int8 public i = -129;
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type int_const -129 is not implicitly convertible to expected type int8.");
}

BOOST_AUTO_TEST_CASE(negative_integers_to_signed_min)
{
	char const* sourceCode = R"(
		contract test {
			int8 public i = -128;
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(positive_integers_to_signed_out_of_bound)
{
	char const* sourceCode = R"(
		contract test {
			int8 public j = 128;
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type int_const 128 is not implicitly convertible to expected type int8.");
}

BOOST_AUTO_TEST_CASE(positive_integers_to_signed_out_of_bound_max)
{
	char const* sourceCode = R"(
		contract test {
			int8 public j = 127;
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(negative_integers_to_unsigned)
{
	char const* sourceCode = R"(
		contract test {
			uint8 public x = -1;
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type int_const -1 is not implicitly convertible to expected type uint8.");
}

BOOST_AUTO_TEST_CASE(positive_integers_to_unsigned_out_of_bound)
{
	char const* sourceCode = R"(
		contract test {
			uint8 public x = 700;
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type int_const 700 is not implicitly convertible to expected type uint8.");
}

BOOST_AUTO_TEST_CASE(integer_boolean_operators)
{
	char const* sourceCode1 = R"(
		contract test { function() public { uint256 x = 1; uint256 y = 2; x || y; } }
	)";
	CHECK_ERROR(sourceCode1, TypeError, "Operator || not compatible with types uint256 and uint256");
	sourceCode1 = R"(
		contract test { function() public { uint x = 1; uint y = 2; x || y; } }
	)";
	CHECK_ERROR(sourceCode1, TypeError, "Operator || not compatible with types uint and uint");
	char const* sourceCode2 = R"(
		contract test { function() public { uint256 x = 1; uint256 y = 2; x && y; } }
	)";
	CHECK_ERROR(sourceCode2, TypeError, "Operator && not compatible with types uint256 and uint256");
	sourceCode2 = R"(
		contract test { function() public { uint x = 1; uint y = 2; x && y; } }
	)";
	CHECK_ERROR(sourceCode2, TypeError, "Operator && not compatible with types uint and uint");
	char const* sourceCode3 = R"(
		contract test { function() public { uint256 x = 1; !x; } }
	)";
	CHECK_ERROR(sourceCode3, TypeError, "Unary operator ! cannot be applied to type uint256");
	sourceCode3 = R"(
		contract test { function() public { uint x = 1; !x; } }
	)";
	CHECK_ERROR(sourceCode3, TypeError, "Unary operator ! cannot be applied to type uint");
}

BOOST_AUTO_TEST_CASE(exp_signed_variable)
{
	char const* sourceCode1 = R"(
		contract test { function() public { uint256 x = 3; int256 y = -4; x ** y; } }
	)";
	CHECK_ERROR(sourceCode1, TypeError, "Operator ** not compatible with types uint256 and int256");
	sourceCode1 = R"(
		contract test { function() public { uint x = 3; int y = -4; x ** y; } }
	)";
	CHECK_ERROR(sourceCode1, TypeError, "Operator ** not compatible with types uint and int");
	char const* sourceCode2 = R"(
		contract test { function() public { uint256 x = 3; int256 y = -4; y ** x; } }
	)";
	CHECK_ERROR(sourceCode2, TypeError, "Operator ** not compatible with types int256 and uint256");
	sourceCode2 = R"(
		contract test { function() public { uint x = 3; int y = -4; y ** x; } }
	)";
	CHECK_ERROR(sourceCode2, TypeError, "Operator ** not compatible with types int and uint");
	char const* sourceCode3 = R"(
		contract test { function() public { int256 x = -3; int256 y = -4; x ** y; } }
	)";
	CHECK_ERROR(sourceCode3, TypeError, "Operator ** not compatible with types int256 and int256");
	sourceCode3 = R"(
		contract test { function() public { int x = -3; int y = -4; x ** y; } }
	)";
	CHECK_ERROR(sourceCode3, TypeError, "Operator ** not compatible with types int and int");
}

BOOST_AUTO_TEST_CASE(reference_compare_operators)
{
	char const* sourceCode1 = R"(
		contract test { bytes a; bytes b; function() public { a == b; } }
	)";
	CHECK_ERROR(sourceCode1, TypeError, "Operator == not compatible with types bytes storage ref and bytes storage ref");
	char const* sourceCode2 = R"(
		contract test { struct s {uint a;} s x; s y; function() public { x == y; } }
	)";
	CHECK_ERROR(sourceCode2, TypeError, "Operator == not compatible with types struct test.s storage ref and struct test.s storage ref");
}

BOOST_AUTO_TEST_CASE(overwrite_memory_location_external)
{
	char const* sourceCode = R"(
		contract C {
			function f(uint[] memory a) external {}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Location has to be calldata for external functions (remove the \"memory\" or \"storage\" keyword).");
}

BOOST_AUTO_TEST_CASE(overwrite_storage_location_external)
{
	char const* sourceCode = R"(
		contract C {
			function f(uint[] storage a) external {}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Location has to be calldata for external functions (remove the \"memory\" or \"storage\" keyword).");
}

BOOST_AUTO_TEST_CASE(storage_location_local_variables)
{
	char const* sourceCode = R"(
		contract C {
			function f() public {
				uint[] storage x;
				uint[] memory y;
				uint[] memory z;
				x;y;z;
			}
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(no_mappings_in_memory_array)
{
	char const* sourceCode = R"(
		contract C {
			function f() public {
				mapping(uint=>uint)[] memory x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type mapping(uint => uint)[] memory is only valid in storage.");
}

BOOST_AUTO_TEST_CASE(assignment_mem_to_local_storage_variable)
{
	char const* sourceCode = R"(
		contract C {
			uint[] data;
			function f(uint[] x) public {
				var dataRef = data;
				dataRef = x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type uint[] memory is not implicitly convertible to expected type uint[] storage pointer.");
}

BOOST_AUTO_TEST_CASE(storage_assign_to_different_local_variable)
{
	char const* sourceCode = R"(
		contract C {
			uint[] data;
			uint8[] otherData;
			function f() public {
				uint8[] storage x = otherData;
				uint[] storage y = data;
				y = x;
				// note that data = otherData works
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type uint8[] storage pointer is not implicitly convertible to expected type uint[] storage pointer.");
}

BOOST_AUTO_TEST_CASE(uninitialized_mapping_variable)
{
	char const* sourceCode = R"(
		contract C {
			function f() public {
				mapping(uint => uint) x;
				x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Uninitialized mapping. Mappings cannot be created dynamically, you have to assign them from a state variable");
}

BOOST_AUTO_TEST_CASE(uninitialized_mapping_array_variable)
{
	char const* sourceCode = R"(
		contract C {
			function f() pure public {
				mapping(uint => uint)[] storage x;
				x;
			}
		}
	)";
	CHECK_WARNING(sourceCode, "Uninitialized storage pointer");
}

BOOST_AUTO_TEST_CASE(uninitialized_mapping_array_variable_050)
{
	char const* sourceCode = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() pure public {
				mapping(uint => uint)[] storage x;
				x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, DeclarationError, "Uninitialized storage pointer");
}

BOOST_AUTO_TEST_CASE(no_delete_on_storage_pointers)
{
	char const* sourceCode = R"(
		contract C {
			uint[] data;
			function f() public {
				var x = data;
				delete x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Unary operator delete cannot be applied to type uint[] storage pointer");
}

BOOST_AUTO_TEST_CASE(assignment_mem_storage_variable_directly)
{
	char const* sourceCode = R"(
		contract C {
			uint[] data;
			function f(uint[] x) public {
				data = x;
			}
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(function_argument_mem_to_storage)
{
	char const* sourceCode = R"(
		contract C {
			function f(uint[] storage x) private {
			}
			function g(uint[] x) public {
				f(x);
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Invalid type for argument in function call. Invalid implicit conversion from uint[] memory to uint[] storage pointer requested.");
}

BOOST_AUTO_TEST_CASE(function_argument_storage_to_mem)
{
	char const* sourceCode = R"(
		contract C {
			function f(uint[] storage x) private {
				g(x);
			}
			function g(uint[] x) public {
			}
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(mem_array_assignment_changes_base_type)
{
	// Such an assignment is possible in storage, but not in memory
	// (because it would incur an otherwise unnecessary copy).
	// This requirement might be lifted, though.
	char const* sourceCode = R"(
		contract C {
			function f(uint8[] memory x) private {
				uint256[] memory y = x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type uint8[] memory is not implicitly convertible to expected type uint256[] memory.");
	sourceCode = R"(
		contract C {
			function f(uint8[] memory x) private {
				uint[] memory y = x;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Type uint8[] memory is not implicitly convertible to expected type uint[] memory.");
}

BOOST_AUTO_TEST_CASE(dynamic_return_types_not_possible)
{
	char const* sourceCode = R"(
		contract C {
			function f(uint) public returns (string);
			function g() public {
				var (x,) = this.f(2);
				// we can assign to x but it is not usable.
				bytes(x).length;
			}
		}
	)";
	if (dev::test::Options::get().evmVersion() == EVMVersion::homestead())
		CHECK_ERROR(sourceCode, TypeError, "Explicit type conversion not allowed from \"inaccessible dynamic type\" to \"bytes storage pointer\".");
	else
		CHECK_WARNING(sourceCode, "Use of the \"var\" keyword is deprecated");
}

BOOST_AUTO_TEST_CASE(memory_arrays_not_resizeable)
{
	char const* sourceCode = R"(
		contract C {
			function f() public {
				uint[] memory x;
				x.length = 2;
			}
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Expression has to be an lvalue.");
}

BOOST_AUTO_TEST_CASE(struct_constructor)
{
	char const* sourceCode = R"(
		contract C {
			struct S { uint a; bool x; }
			function f() public {
				S memory s = S(1, true);
			}
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(struct_constructor_nested)
{
	char const* sourceCode = R"(
		contract C {
			struct X { uint x1; uint x2; }
			struct S { uint s1; uint[3] s2; X s3; }
			function f() public {
				uint[3] memory s2;
				S memory s = S(1, s2, X(4, 5));
			}
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(struct_named_constructor)
{
	char const* sourceCode = R"(
		contract C {
			struct S { uint a; bool x; }
			function f() public {
				S memory s = S({a: 1, x: true});
			}
		}
	)";
	BOOST_CHECK_NO_THROW(parseAndAnalyse(sourceCode));
}

BOOST_AUTO_TEST_CASE(literal_strings)
{
	char const* text = R"(
		contract Foo {
			function f() public {
				string memory long = "01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
				string memory short = "123";
				long; short;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(string_bytes_conversion)
{
	char const* text = R"(
		contract Test {
			string s;
			bytes b;
			function h(string _s) external { bytes(_s).length; }
			function i(string _s) internal { bytes(_s).length; }
			function j() internal { bytes(s).length; }
			function k(bytes _b) external { string(_b); }
			function l(bytes _b) internal { string(_b); }
			function m() internal { string(b); }
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inheriting_from_library)
{
	char const* text = R"(
		library Lib {}
		contract Test is Lib {}
	)";
	CHECK_ERROR(text, TypeError, "Libraries cannot be inherited from.");
}

BOOST_AUTO_TEST_CASE(inheriting_library)
{
	char const* text = R"(
		contract Test {}
		library Lib is Test {}
	)";
	CHECK_ERROR(text, TypeError, "Library is not allowed to inherit.");
}

BOOST_AUTO_TEST_CASE(library_having_variables)
{
	char const* text = R"(
		library Lib { uint x; }
	)";
	CHECK_ERROR(text, TypeError, "Library cannot have non-constant state variables");
}

BOOST_AUTO_TEST_CASE(valid_library)
{
	char const* text = R"(
		library Lib { uint constant x = 9; }
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(call_to_library_function)
{
	char const* text = R"(
		library Lib {
			function min(uint a, uint b) public returns (uint) { if (a < b) return a; return b; }
		}
		contract Test {
			function f() public {
				uint t = Lib.min(12, 7);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(call_to_library_function_undefined)
{
	char const* text = R"(
		library Lib {
			function min(uint a, uint b) public returns (uint);
		}
		contract Test {
			function f() public {
				uint t = Lib.min(12, 7);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "External library functions without an implementation are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}


BOOST_AUTO_TEST_CASE(creating_contract_within_the_contract)
{
	char const* sourceCode = R"(
		contract Test {
			function f() public { var x = new Test(); }
		}
	)";
	CHECK_ERROR(sourceCode, TypeError, "Circular reference for contract creation (cannot create instance of derived or same contract).");
}

BOOST_AUTO_TEST_CASE(array_out_of_bound_access)
{
	char const* text = R"(
		contract c {
			uint[2] dataArray;
			function set5th() public returns (bool) {
				dataArray[5] = 2;
				return true;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Out of bounds array access.");
}

BOOST_AUTO_TEST_CASE(literal_string_to_storage_pointer)
{
	char const* text = R"(
		contract C {
			function f() public { string x = "abc"; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Type literal_string \"abc\" is not implicitly convertible to expected type string storage pointer.");
}

BOOST_AUTO_TEST_CASE(non_initialized_references)
{
	char const* text = R"(
		contract c
		{
			struct s {
				uint a;
			}
			function f() public {
				s storage x;
				x.a = 2;
			}
		}
	)";

	CHECK_WARNING(text, "Uninitialized storage pointer");
}

BOOST_AUTO_TEST_CASE(non_initialized_references_050)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract c
		{
			struct s {
				uint a;
			}
			function f() public {
				s storage x;
			}
		}
	)";

	CHECK_ERROR(text, DeclarationError, "Uninitialized storage pointer");
}

BOOST_AUTO_TEST_CASE(keccak256_with_large_integer_constant)
{
	char const* text = R"(
		contract c
		{
			function f() public { keccak256(2**500); }
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(cyclic_binary_dependency)
{
	char const* text = R"(
		contract A { function f() public { new B(); } }
		contract B { function f() public { new C(); } }
		contract C { function f() public { new A(); } }
	)";
	CHECK_ERROR(text, TypeError, "Circular reference for contract creation (cannot create instance of derived or same contract).");
}

BOOST_AUTO_TEST_CASE(cyclic_binary_dependency_via_inheritance)
{
	char const* text = R"(
		contract A is B { }
		contract B { function f() public { new C(); } }
		contract C { function f() public { new A(); } }
	)";
	CHECK_ERROR(text, TypeError, "Definition of base has to precede definition of derived contract");
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_fail)
{
	char const* text = R"(
		contract C { function f() public { var (x,y); x = 1; y = 1;} }
	)";
	CHECK_ERROR(text, TypeError, "Assignment necessary for type detection.");
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fine)
{
	char const* text = R"(
		contract C {
			function three() public returns (uint, uint, uint);
			function two() public returns (uint, uint);
			function none();
			function f() public {
				var (a,) = three();
				var (b,c,) = two();
				var (,d) = three();
				var (,e,g) = two();
				var (,,) = three();
				var () = none();
				a;b;c;d;e;g;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fail_1)
{
	char const* text = R"(
		contract C {
			function one() public returns (uint);
			function f() public { var (a, b, ) = one(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Not enough components (1) in value to assign all variables (2).");
}
BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fail_2)
{
	char const* text = R"(
		contract C {
			function one() public returns (uint);
			function f() public { var (a, , ) = one(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Not enough components (1) in value to assign all variables (2).");
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fail_3)
{
	char const* text = R"(
		contract C {
			function one() public returns (uint);
			function f() public { var (, , a) = one(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Not enough components (1) in value to assign all variables (2).");
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fail_4)
{
	char const* text = R"(
		contract C {
			function one() public returns (uint);
			function f() public { var (, a, b) = one(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Not enough components (1) in value to assign all variables (2).");
}

BOOST_AUTO_TEST_CASE(tuples)
{
	char const* text = R"(
		contract C {
			function f() public {
				uint a = (1);
				var (b,) = (uint8(1),);
				var (c,d) = (uint32(1), 2 + a);
				var (e,) = (uint64(1), 2, b);
				a;b;c;d;e;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(tuples_empty_components)
{
	char const* text = R"(
		contract C {
			function f() public {
				(1,,2);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Tuple component cannot be empty.");
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fail_5)
{
	char const* text = R"(
		contract C {
			function one() public returns (uint);
			function f() public { var (,) = one(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Wildcard both at beginning and end of variable declaration list is only allowed if the number of components is equal.");
}

BOOST_AUTO_TEST_CASE(multi_variable_declaration_wildcards_fail_6)
{
	char const* text = R"(
		contract C {
			function two() public returns (uint, uint);
			function f() public { var (a, b, c) = two(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Not enough components (2) in value to assign all variables (3)");
}

BOOST_AUTO_TEST_CASE(tuple_assignment_from_void_function)
{
	char const* text = R"(
		contract C {
			function f() public { }
			function g() public {
				var (x,) = (f(), f());
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Cannot declare variable with void (empty tuple) type.");
}

BOOST_AUTO_TEST_CASE(tuple_compound_assignment)
{
	char const* text = R"(
		contract C {
			function f() public returns (uint a, uint b) {
				(a, b) += (1, 1);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Compound assignment is not allowed for tuple types.");
}

BOOST_AUTO_TEST_CASE(member_access_parser_ambiguity)
{
	char const* text = R"(
		contract C {
			struct R { uint[10][10] y; }
			struct S { uint a; uint b; uint[20][20][20] c; R d; }
			S data;
			function f() public {
				C.S x = data;
				C.S memory y;
				C.S[10] memory z;
				C.S[10];
				y.a = 2;
				x.c[1][2][3] = 9;
				x.d.y[2][2] = 3;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_library)
{
	char const* text = R"(
		library D { }
		contract C {
			using D for uint;
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_not_library)
{
	char const* text = R"(
		contract D { }
		contract C {
			using D for uint;
		}
	)";
	CHECK_ERROR(text, TypeError, "Library name expected.");
}

BOOST_AUTO_TEST_CASE(using_for_function_exists)
{
	char const* text = R"(
		library D { function double(uint self) public returns (uint) { return 2*self; } }
		contract C {
			using D for uint;
			function f(uint a) public {
				a.double;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_function_on_int)
{
	char const* text = R"(
		library D { function double(uint self) public returns (uint) { return 2*self; } }
		contract C {
			using D for uint;
			function f(uint a) public returns (uint) {
				return a.double();
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_function_on_struct)
{
	char const* text = R"(
		library D { struct s { uint a; } function mul(s storage self, uint x) public returns (uint) { return self.a *= x; } }
		contract C {
			using D for D.s;
			D.s x;
			function f(uint a) public returns (uint) {
				return x.mul(a);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_overload)
{
	char const* text = R"(
		library D {
			struct s { uint a; }
			function mul(s storage self, uint x) public returns (uint) { return self.a *= x; }
			function mul(s storage, bytes32) public returns (bytes32) { }
		}
		contract C {
			using D for D.s;
			D.s x;
			function f(uint a) public returns (uint) {
				return x.mul(a);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_by_name)
{
	char const* text = R"(
		library D { struct s { uint a; } function mul(s storage self, uint x) public returns (uint) { return self.a *= x; } }
		contract C {
			using D for D.s;
			D.s x;
			function f(uint a) public returns (uint) {
				return x.mul({x: a});
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_mismatch)
{
	char const* text = R"(
		library D { function double(bytes32 self) public returns (uint) { return 2; } }
		contract C {
			using D for uint;
			function f(uint a) public returns (uint) {
				return a.double();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"double\" not found or not visible after argument-dependent lookup in uint");
}

BOOST_AUTO_TEST_CASE(using_for_not_used)
{
	// This is an error because the function is only bound to uint.
	// Had it been bound to *, it would have worked.
	char const* text = R"(
		library D { function double(uint self) public returns (uint) { return 2; } }
		contract C {
			using D for uint;
			function f(uint16 a) public returns (uint) {
				return a.double();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"double\" not found or not visible after argument-dependent lookup in uint16");
}

BOOST_AUTO_TEST_CASE(library_memory_struct)
{
	char const* text = R"(
		pragma experimental ABIEncoderV2;
		library c {
			struct S { uint x; }
			function f() public returns (S ) {}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_for_arbitrary_mismatch)
{
	// Bound to a, but self type does not match.
	char const* text = R"(
		library D { function double(bytes32 self) public returns (uint) { return 2; } }
		contract C {
			using D for *;
			function f(uint a) public returns (uint) {
				return a.double();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"double\" not found or not visible after argument-dependent lookup in uint");
}

BOOST_AUTO_TEST_CASE(bound_function_in_var)
{
	char const* text = R"(
		library D { struct s { uint a; } function mul(s storage self, uint x) public returns (uint) { return self.a *= x; } }
		contract C {
			using D for D.s;
			D.s x;
			function f(uint a) public returns (uint) {
				var g = x.mul;
				return g({x: a});
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(create_memory_arrays)
{
	char const* text = R"(
		library L {
			struct R { uint[10][10] y; }
			struct S { uint a; uint b; uint[20][20][20] c; R d; }
		}
		contract C {
			function f(uint size) public {
				L.S[][] memory x = new L.S[][](10);
				var y = new uint[](20);
				var z = new bytes(size);
				x;y;z;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(mapping_in_memory_array)
{
	char const* text = R"(
		contract C {
			function f(uint size) public {
				var x = new mapping(uint => uint)[](4);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type cannot live outside storage.");
}

BOOST_AUTO_TEST_CASE(new_for_non_array)
{
	char const* text = R"(
		contract C {
			function f(uint size) public {
				var x = new uint(7);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Contract or array type expected.");
}

BOOST_AUTO_TEST_CASE(invalid_args_creating_memory_array)
{
	char const* text = R"(
		contract C {
			function f(uint size) public {
				var x = new uint[]();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Wrong argument count for function call: 0 arguments given but expected 1.");
}

BOOST_AUTO_TEST_CASE(invalid_args_creating_struct)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; }

			function f() public {
				var s = S({a: 1});
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Wrong argument count for struct constructor: 1 arguments given but expected 2.");
}

BOOST_AUTO_TEST_CASE(function_overload_array_type)
{
	char const* text = R"(
			contract M {
				function f(uint[]);
				function f(int[]);
			}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_declaration_and_passing_implicit_conversion)
{
	char const* text = R"(
			contract C {
				function f() public returns (uint) {
					uint8 x = 7;
					uint16 y = 8;
					uint32 z = 9;
					uint32[3] memory ending = [x, y, z];
					return (ending[1]);
				}
			}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_declaration_and_passing_implicit_conversion_strings)
{
	char const* text = R"(
		contract C {
			function f() public returns (string) {
				string memory x = "Hello";
				string memory y = "World";
				string[2] memory z = [x, y];
				return (z[0]);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_declaration_const_int_conversion)
{
	char const* text = R"(
		contract C {
			function f() public returns (uint) {
				uint8[4] memory z = [1,2,3,5];
				return (z[0]);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_declaration_const_string_conversion)
{
	char const* text = R"(
		contract C {
			function f() public returns (string) {
				string[2] memory z = ["Hello", "World"];
				return (z[0]);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_declaration_no_type)
{
	char const* text = R"(
		contract C {
			function f() public returns (uint) {
				return ([4,5,6][1]);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_declaration_no_type_strings)
{
	char const* text = R"(
		contract C {
			function f() public returns (string) {
				return (["foo", "man", "choo"][1]);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_struct_declaration_arrays)
{
	char const* text = R"(
		contract C {
			struct S {
				uint a;
				string b;
			}
			function f() {
				S[2] memory x = [S({a: 1, b: "fish"}), S({a: 2, b: "fish"})];
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(invalid_types_in_inline_array)
{
	char const* text = R"(
		contract C {
			function f() public {
				uint[3] x = [45, 'foo', true];
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Unable to deduce common type for array elements.");
}

BOOST_AUTO_TEST_CASE(dynamic_inline_array)
{
	char const* text = R"(
		contract C {
			function f() public {
				uint8[4][4] memory dyn = [[1, 2, 3, 4], [2, 3, 4, 5], [3, 4, 5, 6], [4, 5, 6, 7]];
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(lvalues_as_inline_array)
{
	char const* text = R"(
		contract C {
			function f() public {
				[1, 2, 3]++;
				[1, 2, 3] = [4, 5, 6];
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Inline array type cannot be declared as LValue.");
}

BOOST_AUTO_TEST_CASE(break_not_in_loop)
{
	char const* text = R"(
		contract C {
			function f() public {
				if (true)
					break;
			}
		}
	)";
	CHECK_ERROR(text, SyntaxError, "\"break\" has to be in a \"for\" or \"while\" loop.");
}

BOOST_AUTO_TEST_CASE(continue_not_in_loop)
{
	char const* text = R"(
		contract C {
			function f() public {
				if (true)
					continue;
			}
		}
	)";
	CHECK_ERROR(text, SyntaxError, "\"continue\" has to be in a \"for\" or \"while\" loop.");
}

BOOST_AUTO_TEST_CASE(continue_not_in_loop_2)
{
	char const* text = R"(
		contract C {
			function f() public {
				while (true)
				{
				}
				continue;
			}
		}
	)";
	CHECK_ERROR(text, SyntaxError, "\"continue\" has to be in a \"for\" or \"while\" loop.");
}

BOOST_AUTO_TEST_CASE(invalid_different_types_for_conditional_expression)
{
	char const* text = R"(
		contract C {
			function f() public {
				true ? true : 2;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "True expression's type bool doesn't match false expression's type uint8.");
}

BOOST_AUTO_TEST_CASE(left_value_in_conditional_expression_not_supported_yet)
{
	char const* text = R"(
		contract C {
			function f() public {
				uint x;
				uint y;
				(true ? x : y) = 1;
			}
		}
	)";
	CHECK_ERROR_ALLOW_MULTI(text, TypeError, (std::vector<std::string>{
		"Conditional expression as left value is not supported yet.",
		"Expression has to be an lvalue"
	}));
}

BOOST_AUTO_TEST_CASE(conditional_expression_with_different_struct)
{
	char const* text = R"(
		contract C {
			struct s1 {
				uint x;
			}
			struct s2 {
				uint x;
			}
			function f() public {
				s1 memory x;
				s2 memory y;
				true ? x : y;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "True expression's type struct C.s1 memory doesn't match false expression's type struct C.s2 memory.");
}

BOOST_AUTO_TEST_CASE(conditional_expression_with_different_function_type)
{
	char const* text = R"(
		contract C {
			function x(bool) public {}
			function y() public {}

			function f() public {
				true ? x : y;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "True expression's type function (bool) doesn't match false expression's type function ().");
}

BOOST_AUTO_TEST_CASE(conditional_expression_with_different_enum)
{
	char const* text = R"(
		contract C {
			enum small { A, B, C, D }
			enum big { A, B, C, D }

			function f() public {
				small x;
				big y;

				true ? x : y;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "True expression's type enum C.small doesn't match false expression's type enum C.big.");
}

BOOST_AUTO_TEST_CASE(conditional_expression_with_different_mapping)
{
	char const* text = R"(
		contract C {
			mapping(uint8 => uint8) table1;
			mapping(uint32 => uint8) table2;

			function f() public {
				true ? table1 : table2;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "True expression's type mapping(uint8 => uint8) doesn't match false expression's type mapping(uint32 => uint8).");
}

BOOST_AUTO_TEST_CASE(conditional_with_all_types)
{
	char const* text = R"(
		contract C {
			struct s1 {
				uint x;
			}
			s1 struct_x;
			s1 struct_y;

			function fun_x() public {}
			function fun_y() public {}

			enum small { A, B, C, D }

			mapping(uint8 => uint8) table1;
			mapping(uint8 => uint8) table2;

			function f() public {
				// integers
				uint x;
				uint y;
				uint g = true ? x : y;
				g += 1; // Avoid unused var warning

				// integer constants
				uint h = true ? 1 : 3;
				h += 1; // Avoid unused var warning

				// string literal
				var i = true ? "hello" : "world";
				i = "used"; //Avoid unused var warning
			}
			function f2() public {
				// bool
				bool j = true ? true : false;
				j = j && true; // Avoid unused var warning

				// real is not there yet.

				// array
				byte[2] memory a;
				byte[2] memory b;
				var k = true ? a : b;
				k[0] = byte(0); //Avoid unused var warning

				bytes memory e;
				bytes memory f;
				var l = true ? e : f;
				l[0] = byte(0); // Avoid unused var warning

				// fixed bytes
				bytes2 c;
				bytes2 d;
				var m = true ? c : d;
				m &= m;

			}
			function f3() public {
				// contract doesn't fit in here

				// struct
				struct_x = true ? struct_x : struct_y;

				// function
				var r = true ? fun_x : fun_y;
				r(); // Avoid unused var warning
				// enum
				small enum_x;
				small enum_y;
				enum_x = true ? enum_x : enum_y;

				// tuple
				var (n, o) = true ? (1, 2) : (3, 4);
				(n, o) = (o, n); // Avoid unused var warning
				// mapping
				var p = true ? table1 : table2;
				p[0] = 0; // Avoid unused var warning
				// typetype
				var q = true ? uint32(1) : uint32(2);
				q += 1; // Avoid unused var warning
				// modifier doesn't fit in here

				// magic doesn't fit in here

				// module doesn't fit in here
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(index_access_for_bytes)
{
	char const* text = R"(
		contract C {
			bytes20 x;
			function f(bytes16 b) public {
				b[uint(x[2])];
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(uint7_and_uintM_as_identifier)
{
	char const* text = R"(
		contract test {
		string uintM = "Hello 4 you";
			function f() public {
				uint8 uint7 = 3;
				uint7 = 5;
				string memory intM;
				uint bytesM = 21;
				intM; bytesM;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(varM_disqualified_as_keyword)
{
	char const* text = R"(
		contract test {
			function f() public {
				uintM something = 3;
				intM should = 4;
				bytesM fail = "now";
			}
		}
	)";
	CHECK_ERROR_ALLOW_MULTI(text, DeclarationError, (std::vector<std::string>{
		"Identifier not found or not unique.",
		"Identifier not found or not unique.",
		"Identifier not found or not unique."
	}));
}

BOOST_AUTO_TEST_CASE(modifier_is_not_a_valid_typename)
{
	char const* text = R"(
		contract test {
			modifier mod() { _; }

			function f() public {
				mod g;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Name has to refer to a struct, enum or contract.");
}

BOOST_AUTO_TEST_CASE(modifier_is_not_a_valid_typename_is_not_fatal)
{
	char const* text = R"(
		contract test {
			modifier mod() { _; }

			function f() public {
				mod g;
				g = f;
			}
		}
	)";
	CHECK_ERROR_ALLOW_MULTI(text, TypeError, (std::vector<std::string>{"Name has to refer to a struct, enum or contract."}));
}

BOOST_AUTO_TEST_CASE(function_is_not_a_valid_typename)
{
	char const* text = R"(
		contract test {
			function foo() public {
			}

			function f() public {
				foo g;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Name has to refer to a struct, enum or contract.");
}

BOOST_AUTO_TEST_CASE(long_uint_variable_fails)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint99999999999999999999999999 something = 3;
			}
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found or not unique.");
}

BOOST_AUTO_TEST_CASE(bytes10abc_is_identifier)
{
	char const* text = R"(
		contract test {
			function f() public {
				bytes32 bytes10abc = "abc";
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(int10abc_is_identifier)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint uint10abc = 3;
				int int10abc = 4;
				uint10abc; int10abc;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(library_functions_do_not_have_value)
{
	char const* text = R"(
		library L { function l() public {} }
		contract test {
			function f() public {
				L.l.value;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup in function ()");
}

BOOST_AUTO_TEST_CASE(invalid_fixed_types_0x7_mxn)
{
	char const* text = R"(
		contract test {
			fixed0x7 a = .3;
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found");
}

BOOST_AUTO_TEST_CASE(invalid_fixed_types_long_invalid_identifier)
{
	char const* text = R"(
		contract test {
			fixed99999999999999999999999999999999999999x7 b = 9.5;
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found");
}

BOOST_AUTO_TEST_CASE(invalid_fixed_types_7x8_mxn)
{
	char const* text = R"(
		contract test {
			fixed7x8 c = 3.12345678;
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found");
}

BOOST_AUTO_TEST_CASE(library_instances_cannot_be_used)
{
	char const* text = R"(
		library L { function l() public {} }
		contract test {
			function f() public {
				L x;
				x.l();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"l\" not found or not visible after argument-dependent lookup in library L");
}

BOOST_AUTO_TEST_CASE(invalid_fixed_type_long)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed8x888888888888888888888888888888888888888888888888888 b;
			}
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found");
}

BOOST_AUTO_TEST_CASE(fixed_type_int_conversion)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint64 a = 3;
				int64 b = 4;
				fixed c = b;
				ufixed d = a;
				c; d;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(fixed_type_rational_int_conversion)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed c = 3;
				ufixed d = 4;
				c; d;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(fixed_type_rational_fraction_conversion)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed a = 4.5;
				ufixed d = 2.5;
				a; d;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(invalid_int_implicit_conversion_from_fixed)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed a = 4.5;
				int256 b = a;
				a; b;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type fixed128x18 is not implicitly convertible to expected type int256");
	text = R"(
		contract test {
			function f() public {
				fixed a = 4.5;
				int b = a;
				a; b;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type fixed128x18 is not implicitly convertible to expected type int");
}

BOOST_AUTO_TEST_CASE(rational_unary_operation)
{
	char const* text = R"(
		contract test {
			function f() pure public {
				ufixed16x2 a = 3.25;
				fixed16x2 b = -3.25;
				a; b;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);

	// Test deprecation warning under < 0.5.0
	text = R"(
		contract test {
			function f() pure public {
				ufixed16x2 a = +3.25;
				fixed16x2 b = -3.25;
				a; b;
			}
		}
	)";
	CHECK_WARNING(text, "Use of unary + is deprecated");
	text = R"(
		contract test {
			function f(uint x) pure public {
				uint y = +x;
				y;
			}
		}
	)";
	CHECK_WARNING(text,"Use of unary + is deprecated");

	// Test syntax error under 0.5.0
	text = R"(
		pragma experimental "v0.5.0";
		contract test {
			function f() pure public {
				ufixed16x2 a = +3.25;
				fixed16x2 b = -3.25;
				a; b;
			}
		}
	)";
	CHECK_ERROR(text, SyntaxError, "Use of unary + is deprecated");
	text = R"(
		pragma experimental "v0.5.0";
		contract test {
			function f(uint x) pure public {
				uint y = +x;
				y;
			}
		}
	)";
	CHECK_ERROR(text, SyntaxError, "Use of unary + is deprecated");
}

BOOST_AUTO_TEST_CASE(leading_zero_rationals_convert)
{
	char const* text = R"(
		contract A {
			function f() pure public {
				ufixed16x2 a = 0.5;
				ufixed256x52 b = 0.0000000000000006661338147750939242541790008544921875;
				fixed16x2 c = -0.5;
				fixed256x52 d = -0.0000000000000006661338147750939242541790008544921875;
				a; b; c; d;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(size_capabilities_of_fixed_point_types)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed256x1 a = 123456781234567979695948382928485849359686494864095409282048094275023098123.5;
				ufixed256x77 b = 0.920890746623327805482905058466021565416131529487595827354393978494366605267637;
				ufixed224x78 c = 0.000000000001519884736399797998492268541131529487595827354393978494366605267646;
				fixed256x1 d = -123456781234567979695948382928485849359686494864095409282048094275023098123.5;
				fixed256x76 e = -0.93322335481643744342575580035176794825198893968114429702091846411734101080123;
				fixed256x79 g = -0.0001178860664374434257558003517679482519889396811442970209184641173410108012309;
				a; b; c; d; e; g;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(zero_handling)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed16x2 a = 0; a;
				ufixed32x1 b = 0; b;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(fixed_type_invalid_implicit_conversion_size)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed a = 11/4;
				ufixed248x8 b = a; b;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type ufixed128x18 is not implicitly convertible to expected type ufixed248x8");
}

BOOST_AUTO_TEST_CASE(fixed_type_invalid_implicit_conversion_lost_data)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed256x1 a = 1/3; a;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "is not implicitly convertible to expected type ufixed256x1");
}

BOOST_AUTO_TEST_CASE(fixed_type_valid_explicit_conversions)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed256x80 a = ufixed256x80(1/3); a;
				ufixed248x80 b = ufixed248x80(1/3); b;
				ufixed8x1 c = ufixed8x1(1/3); c;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(invalid_array_declaration_with_rational)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint[3.5] a; a;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Array with fractional length specified.");
}

BOOST_AUTO_TEST_CASE(invalid_array_declaration_with_signed_fixed_type)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint[fixed(3.5)] a; a;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Invalid array length, expected integer literal or constant expression.");
}

BOOST_AUTO_TEST_CASE(invalid_array_declaration_with_unsigned_fixed_type)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint[ufixed(3.5)] a; a;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Invalid array length, expected integer literal or constant expression.");
}

BOOST_AUTO_TEST_CASE(rational_to_bytes_implicit_conversion)
{
	char const* text = R"(
		contract test {
			function f() public {
				bytes32 c = 3.2; c;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "is not implicitly convertible to expected type bytes32");
}

BOOST_AUTO_TEST_CASE(fixed_to_bytes_implicit_conversion)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed a = 3.25;
				bytes32 c = a; c;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "fixed128x18 is not implicitly convertible to expected type bytes32");
}

BOOST_AUTO_TEST_CASE(mapping_with_fixed_literal)
{
	char const* text = R"(
		contract test {
			mapping(ufixed8x1 => string) fixedString;
			function f() public {
				fixedString[0.5] = "Half";
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(fixed_points_inside_structs)
{
	char const* text = R"(
		contract test {
			struct myStruct {
				ufixed a;
				int b;
			}
			myStruct a = myStruct(3.125, 3);
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_fixed_types)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed[3] memory a = [fixed(3.5), fixed(-4.25), fixed(967.125)];
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(inline_array_rationals)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed128x3[4] memory a = [ufixed128x3(3.5), 4.125, 2.5, 4.0];
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(rational_index_access)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint[] memory a;
				a[.5];
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "rational_const 1 / 2 is not implicitly convertible to expected type uint");
}

BOOST_AUTO_TEST_CASE(rational_to_fixed_literal_expression)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed64x8 a = 3.5 * 3;
				ufixed64x8 b = 4 - 2.5;
				ufixed64x8 c = 11 / 4;
				ufixed240x5 d = 599 + 0.21875;
				ufixed256x80 e = ufixed256x80(35.245 % 12.9);
				ufixed256x80 f = ufixed256x80(1.2 % 2);
				fixed g = 2 ** -2;
				a; b; c; d; e; f; g;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(rational_as_exponent_value_signed)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed g = 2 ** -2.2;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(rational_as_exponent_value_unsigned)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed b = 3 ** 2.5;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(rational_as_exponent_half)
{
	char const* text = R"(
		contract test {
			function f() public {
				2 ** (1/2);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(rational_as_exponent_value_neg_quarter)
{
	char const* text = R"(
		contract test {
			function f() public {
				42 ** (-1/4);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(fixed_point_casting_exponents_15)
{
	char const* text = R"(
		contract test {
			function f() public {
				var a = 3 ** ufixed(1.5);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(fixed_point_casting_exponents_neg)
{
	char const* text = R"(
		contract test {
			function f() public {
				var c = 42 ** fixed(-1/4);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(var_capable_of_holding_constant_rationals)
{
	char const* text = R"(
		contract test {
			function f() public {
				var a = 0.12345678;
				var b = 12345678.352;
				var c = 0.00000009;
				a; b; c;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(var_and_rational_with_tuple)
{
	char const* text = R"(
		contract test {
			function f() public {
				var (a, b) = (.5, 1/3);
				a; b;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(var_handle_divided_integers)
{
	char const* text = R"(
		contract test {
			function f() public {
				var x = 1/3;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(rational_bitnot_unary_operation)
{
	char const* text = R"(
		contract test {
			function f() public {
				~fixed(3.5);
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "cannot be applied");
}

BOOST_AUTO_TEST_CASE(rational_bitor_binary_operation)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed(1.5) | 3;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(rational_bitxor_binary_operation)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed(1.75) ^ 3;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(rational_bitand_binary_operation)
{
	char const* text = R"(
		contract test {
			function f() public {
				fixed(1.75) & 3;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "not compatible with types");
}

BOOST_AUTO_TEST_CASE(missing_bool_conversion)
{
	char const* text = R"(
		contract test {
			function b(uint a) public {
				bool(a == 1);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(integer_and_fixed_interaction)
{
	char const* text = R"(
		contract test {
			function f() public {
				ufixed a = uint64(1) + ufixed(2);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(one_divided_by_three_integer_conversion)
{
	char const* text = R"(
		contract test {
			function f() public {
				uint256 a = 1/3;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "is not implicitly convertible to expected type uint256. Try converting to type ufixed256x77");
	text = R"(
		contract test {
			function f() public {
				uint a = 1/3;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "is not implicitly convertible to expected type uint. Try converting to type ufixed256x77");
}

BOOST_AUTO_TEST_CASE(unused_return_value)
{
	char const* text = R"(
		contract test {
			function g() public returns (uint) {}
			function f() public {
				g();
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(unused_return_value_send)
{
	char const* text = R"(
		contract test {
			function f() public {
				address(0x12).send(1);
			}
		}
	)";
	CHECK_WARNING(text, "Failure condition of 'send' ignored. Consider using 'transfer' instead.");
}

BOOST_AUTO_TEST_CASE(unused_return_value_call)
{
	char const* text = R"(
		contract test {
			function f() public {
				address(0x12).call("abc");
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}

BOOST_AUTO_TEST_CASE(unused_return_value_call_value)
{
	char const* text = R"(
		contract test {
			function f() public {
				address(0x12).call.value(2)("abc");
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}

BOOST_AUTO_TEST_CASE(unused_return_value_callcode)
{
	char const* text = R"(
		contract test {
			function f() public {
				address(0x12).callcode("abc");
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}

BOOST_AUTO_TEST_CASE(unused_return_value_delegatecall)
{
	char const* text = R"(
		contract test {
			function f() public {
				address(0x12).delegatecall("abc");
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}

BOOST_AUTO_TEST_CASE(warn_about_callcode)
{
	char const* text = R"(
		contract test {
			function f() pure public {
				address(0x12).callcode;
			}
		}
	)";
	CHECK_WARNING(text, "\"callcode\" has been deprecated in favour of \"delegatecall\"");
	text = R"(
		pragma experimental "v0.5.0";
		contract test {
			function f() pure public {
				address(0x12).callcode;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "\"callcode\" has been deprecated in favour of \"delegatecall\"");
}

BOOST_AUTO_TEST_CASE(no_warn_about_callcode_as_function)
{
	char const* text = R"(
		contract test {
			function callcode() pure public {
				test.callcode();
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(payable_in_library)
{
	char const* text = R"(
		library test {
			function f() payable public {}
		}
	)";
	CHECK_ERROR(text, TypeError, "Library functions cannot be payable.");
}

BOOST_AUTO_TEST_CASE(payable_external)
{
	char const* text = R"(
		contract test {
			function f() payable external {}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(payable_internal)
{
	char const* text = R"(
		contract test {
			function f() payable internal {}
		}
	)";
	CHECK_ERROR(text, TypeError, "Internal functions cannot be payable.");
}

BOOST_AUTO_TEST_CASE(payable_private)
{
	char const* text = R"(
		contract test {
			function f() payable private {}
		}
	)";
	CHECK_ERROR(text, TypeError, "Internal functions cannot be payable.");
}

BOOST_AUTO_TEST_CASE(illegal_override_payable)
{
	char const* text = R"(
		contract B { function f() payable public {} }
		contract C is B { function f() public {} }
	)";
	CHECK_ERROR(text, TypeError, "Overriding function changes state mutability from \"payable\" to \"nonpayable\".");
}

BOOST_AUTO_TEST_CASE(illegal_override_payable_nonpayable)
{
	char const* text = R"(
		contract B { function f() public {} }
		contract C is B { function f() payable public {} }
	)";
	CHECK_ERROR(text, TypeError, "Overriding function changes state mutability from \"nonpayable\" to \"payable\".");
}

BOOST_AUTO_TEST_CASE(function_variable_mixin)
{
       // bug #1798 (cpp-ethereum), related to #1286 (solidity)
       char const* text = R"(
               contract attribute {
                       bool ok = false;
               }
               contract func {
                       function ok() public returns (bool) { return true; }
               }

               contract attr_func is attribute, func {
                       function checkOk() public returns (bool) { return ok(); }
               }
       )";
       CHECK_ERROR(text, DeclarationError, "Identifier already declared.");
}

BOOST_AUTO_TEST_CASE(calling_payable)
{
	char const* text = R"(
		contract receiver { function pay() payable public {} }
		contract test {
			function f() public { (new receiver()).pay.value(10)(); }
			receiver r = new receiver();
			function g() public { r.pay.value(10)(); }
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(calling_nonpayable)
{
	char const* text = R"(
		contract receiver { function nopay() public {} }
		contract test {
			function f() public { (new receiver()).nopay.value(10)(); }
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup in function () external - did you forget the \"payable\" modifier?");
}

BOOST_AUTO_TEST_CASE(non_payable_constructor)
{
	char const* text = R"(
		contract C {
			function C() { }
		}
		contract D {
			function f() public returns (uint) {
				(new C).value(2)();
				return 2;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup in function () returns (contract C) - did you forget the \"payable\" modifier?");
}

BOOST_AUTO_TEST_CASE(warn_nonpresent_pragma)
{
	char const* text = "contract C {}";
	auto sourceAndError = parseAnalyseAndReturnError(text, true, false);
	BOOST_REQUIRE(!sourceAndError.second.empty());
	BOOST_REQUIRE(!!sourceAndError.first);
	BOOST_CHECK(searchErrorMessage(*sourceAndError.second.front(), "Source file does not specify required compiler version!"));
}

BOOST_AUTO_TEST_CASE(unsatisfied_version)
{
	char const* text = R"(
		pragma solidity ^99.99.0;
	)";
	auto sourceAndError = parseAnalyseAndReturnError(text, false, false, false);
	BOOST_REQUIRE(!sourceAndError.second.empty());
	BOOST_REQUIRE(!!sourceAndError.first);
	BOOST_CHECK(sourceAndError.second.front()->type() == Error::Type::SyntaxError);
	BOOST_CHECK(searchErrorMessage(*sourceAndError.second.front(), "Source file requires different compiler version"));
}

BOOST_AUTO_TEST_CASE(invalid_array_as_statement)
{
	char const* text = R"(
		contract test {
			struct S { uint x; }
			function test(uint k) public { S[k]; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Integer constant expected.");
}

BOOST_AUTO_TEST_CASE(using_directive_for_missing_selftype)
{
	char const* text = R"(
		library B {
			function b() public {}
		}

		contract A {
			using B for bytes;

			function a() public {
				bytes memory x;
				x.b();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"b\" not found or not visible after argument-dependent lookup in bytes memory");
}

BOOST_AUTO_TEST_CASE(shift_constant_left_negative_rvalue)
{
	char const* text = R"(
		contract C {
			uint public a = 0x42 << -8;
		}
	)";
	CHECK_ERROR(text, TypeError, "Operator << not compatible with types int_const 66 and int_const -8");
}

BOOST_AUTO_TEST_CASE(shift_constant_right_negative_rvalue)
{
	char const* text = R"(
		contract C {
			uint public a = 0x42 >> -8;
		}
	)";
	CHECK_ERROR(text, TypeError, "Operator >> not compatible with types int_const 66 and int_const -8");
}

BOOST_AUTO_TEST_CASE(shift_constant_left_excessive_rvalue)
{
	char const* text = R"(
		contract C {
			uint public a = 0x42 << 0x100000000;
		}
	)";
	CHECK_ERROR(text, TypeError, "Operator << not compatible with types int_const 66 and int_const 4294967296");
}

BOOST_AUTO_TEST_CASE(shift_constant_right_excessive_rvalue)
{
	char const* text = R"(
		contract C {
			uint public a = 0x42 >> 0x100000000;
		}
	)";
	CHECK_ERROR(text, TypeError, "Operator >> not compatible with types int_const 66 and int_const 4294967296");
}

BOOST_AUTO_TEST_CASE(shift_constant_right_fractional)
{
	char const* text = R"(
		contract C {
			uint public a = 0x42 >> (1 / 2);
		}
	)";
	CHECK_ERROR(text, TypeError, "Operator >> not compatible with types int_const 66 and rational_const 1 / 2");
}

BOOST_AUTO_TEST_CASE(inline_assembly_unbalanced_positive_stack)
{
	char const* text = R"(
		contract test {
			function f() public {
				assembly {
					1
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_unbalanced_negative_stack)
{
	char const* text = R"(
		contract test {
			function f() public {
				assembly {
					pop
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_unbalanced_two_stack_load)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract c {
			uint8 x;
			function f() public {
				assembly { pop(x) }
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_in_modifier)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			modifier m {
				uint a = 1;
				assembly {
					a := 2
				}
				_;
			}
			function f() public m {
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_storage)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			uint x = 1;
			function f() public {
				assembly {
					x := 2
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_storage_in_modifiers)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			uint x = 1;
			modifier m {
				assembly {
					x := 2
				}
				_;
			}
			function f() public m {
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_constant_assign)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			uint constant x = 1;
			function f() public {
				assembly {
					x := 2
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_constant_access)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			uint constant x = 1;
			function f() public {
				assembly {
					let y := x
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_local_variable_access_out_of_functions)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			function f() public {
				uint a;
				assembly {
					function g() -> x { x := a }
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_local_variable_access_out_of_functions_storage_ptr)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			uint[] r;
			function f() public {
				uint[] storage a = r;
				assembly {
					function g() -> x { x := a_offset }
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_storage_variable_access_out_of_functions)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract test {
			uint a;
			function f() pure public {
				assembly {
					function g() -> x { x := a_slot }
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_constant_variable_via_offset)
{
	char const* text = R"(
		contract test {
			uint constant x = 2;
			function f() pure public {
				assembly {
					let r := x_offset
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}
/*
BOOST_AUTO_TEST_CASE(inline_assembly_calldata_variables)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f(bytes bytesAsCalldata) external {
				assembly {
					let x := bytesAsCalldata
				}
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Call data elements cannot be accessed directly.");
}
*/
BOOST_AUTO_TEST_CASE(inline_assembly_050_literals_on_stack)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() pure public {
				assembly {
					1
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_literals_on_stack)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				assembly {
					1
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_050_bare_instructions)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() view public {
				assembly {
					address
					pop
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_bare_instructions)
{
	char const* text = R"(
		contract C {
			function f() view public {
				assembly {
					address
					pop
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_050_labels)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() pure public {
				assembly {
					label:
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_labels)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				assembly {
					label:
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_050_jump)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() pure public {
				assembly {
					jump(2)
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_jump)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				assembly {
					jump(2)
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_050_leave_items_on_stack)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() pure public {
				assembly {
					mload(0)
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(inline_assembly_leave_items_on_stack)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				assembly {
					mload(0)
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(rational_with_uint_mobile_type)
{
	char const* text = R"(
			contract C {
				function f() public {
					// Invalid number
					[1, 78901234567890123456789012345678901234567890123456789345678901234567890012345678012345678901234567];
				}
			}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(warns_msg_value_in_non_payable_public_function)
{
	char const* text = R"(
		contract C {
			function f() view public {
				msg.value;
			}
		}
	)";
	CHECK_WARNING(text, "\"msg.value\" used in non-payable function. Do you want to add the \"payable\" modifier to this function?");
}

BOOST_AUTO_TEST_CASE(does_not_warn_msg_value_in_payable_function)
{
	char const* text = R"(
		contract C {
			function f() payable public {
				msg.value;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(does_not_warn_msg_value_in_internal_function)
{
	char const* text = R"(
		contract C {
			function f() view internal {
				msg.value;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(does_not_warn_msg_value_in_library)
{
	char const* text = R"(
		library C {
			function f() view public {
				msg.value;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(does_not_warn_msg_value_in_modifier_following_non_payable_public_function)
{
	char const* text = R"(
		contract c {
			function f() pure public { }
			modifier m() { msg.value; _; }
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(assignment_to_constant)
{
	char const* text = R"(
		contract c {
			uint constant a = 1;
			function f() public { a = 2; }
		}
	)";
	CHECK_ERROR(text, TypeError, "Cannot assign to a constant variable.");
}

BOOST_AUTO_TEST_CASE(return_structs)
{
	char const* text = R"(
		pragma experimental ABIEncoderV2;
		contract C {
			struct S { uint a; T[] sub; }
			struct T { uint[] x; }
			function f() returns (uint, S) {
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(read_returned_struct)
{
	char const* text = R"(
		pragma experimental ABIEncoderV2;
		contract A {
			struct T {
				int x;
				int y;
			}
			function g() public returns (T) {
				return this.g();
			}
		}
	)";
	CHECK_WARNING(text, "Experimental features");
}
BOOST_AUTO_TEST_CASE(address_checksum_type_deduction)
{
	char const* text = R"(
		contract C {
			function f() public {
				var x = 0xfA0bFc97E48458494Ccd857e1A85DC91F7F0046E;
				x.send(2);
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(invalid_address_checksum)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				address x = 0xFA0bFc97E48458494Ccd857e1A85DC91F7F0046E;
				x;
			}
		}
	)";
	CHECK_WARNING(text, "This looks like an address but has an invalid checksum.");
}

BOOST_AUTO_TEST_CASE(invalid_address_no_checksum)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				address x = 0xfa0bfc97e48458494ccd857e1a85dc91f7f0046e;
				x;
			}
		}
	)";
	CHECK_WARNING(text, "This looks like an address but has an invalid checksum.");
}

BOOST_AUTO_TEST_CASE(invalid_address_length_short)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				address x = 0xA0bFc97E48458494Ccd857e1A85DC91F7F0046E;
				x;
			}
		}
	)";
	CHECK_WARNING(text, "This looks like an address but has an invalid checksum.");
}

BOOST_AUTO_TEST_CASE(invalid_address_length_long)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				address x = 0xFA0bFc97E48458494Ccd857e1A85DC91F7F0046E0;
				x;
			}
		}
	)";
	CHECK_ALLOW_MULTI(text, (std::vector<std::pair<Error::Type, std::string>>{
		{Error::Type::Warning, "This looks like an address but has an invalid checksum."},
		{Error::Type::TypeError, "not implicitly convertible"}
	}));
}

BOOST_AUTO_TEST_CASE(address_test_for_bug_in_implementation)
{
	// A previous implementation claimed the string would be an address
	char const* text = R"(
		contract AddrString {
			address public test = "0xCA35b7d915458EF540aDe6068dFe2F44E8fa733c";
		}
	)";
	CHECK_ERROR(text, TypeError, "is not implicitly convertible to expected type address");
	text = R"(
		contract AddrString {
			function f() public returns (address) {
				return "0xCA35b7d915458EF540aDe6068dFe2F44E8fa733c";
		   }
		}
	)";
	CHECK_ERROR(text, TypeError, "is not implicitly convertible to expected type");
}

BOOST_AUTO_TEST_CASE(early_exit_on_fatal_errors)
{
	// This tests a crash that occured because we did not stop for fatal errors.
	char const* text = R"(
		contract C {
			struct S {
				ftring a;
			}
			S public s;
			function s() s {
			}
		}
	)";
	CHECK_ERROR(text, DeclarationError, "Identifier not found or not unique");
}

BOOST_AUTO_TEST_CASE(address_methods)
{
	char const* text = R"(
		contract C {
			function f() public {
				address addr;
				uint balance = addr.balance;
				uint16 size = addr.codesize;
				//bool callRet = addr.call();
				//bool callcodeRet = addr.callcode();
				//bool delegatecallRet = addr.delegatecall();
				bool sendRet = addr.send(1);
				addr.transfer(1);
				sendRet;
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(interface)
{
	char const* text = R"(
		interface I {
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(interface_functions)
{
	char const* text = R"(
		interface I {
			function();
			function f();
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(interface_function_bodies)
{
	char const* text = R"(
		interface I {
			function f() public {
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Functions in interfaces cannot have an implementation");
}

BOOST_AUTO_TEST_CASE(interface_events)
{
	char const* text = R"(
		interface I {
			event E();
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(interface_inheritance)
{
	char const* text = R"(
		interface A {
		}
		interface I is A {
		}
	)";
	CHECK_ERROR(text, TypeError, "Interfaces cannot inherit");
}


BOOST_AUTO_TEST_CASE(interface_structs)
{
	char const* text = R"(
		interface I {
			struct A {
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Structs cannot be defined in interfaces");
}

BOOST_AUTO_TEST_CASE(interface_variables)
{
	char const* text = R"(
		interface I {
			uint a;
		}
	)";
	CHECK_ERROR(text, TypeError, "Variables cannot be declared in interfaces");
}

BOOST_AUTO_TEST_CASE(interface_function_parameters)
{
	char const* text = R"(
		interface I {
			function f(uint a) public returns (bool);
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(interface_enums)
{
	char const* text = R"(
		interface I {
			enum A { B, C }
		}
	)";
	CHECK_ERROR(text, TypeError, "Enumerable cannot be declared in interfaces");
}

BOOST_AUTO_TEST_CASE(using_interface)
{
	char const* text = R"(
		interface I {
			function f();
		}
		contract C is I {
			function f() public {
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(using_interface_complex)
{
	char const* text = R"(
		interface I {
			event A();
			function f();
			function g();
			function();
		}
		contract C is I {
			function f() public {
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(interface_implement_public_contract)
{
	char const* text = R"(
		interface I {
			function f() external;
		}
		contract C is I {
			function f() public {
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(warn_about_throw)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				throw;
			}
		}
	)";
	CHECK_WARNING(text, "\"throw\" is deprecated in favour of \"revert()\", \"require()\" and \"assert()\"");
	text = R"(
                pragma experimental "v0.5.0";
		contract C {
			function f() pure public {
				throw;
			}
		}
	)";
	CHECK_ERROR(text, SyntaxError, "\"throw\" is deprecated in favour of \"revert()\", \"require()\" and \"assert()\"");
}

BOOST_AUTO_TEST_CASE(bare_revert)
{
	char const* text = R"(
		contract C {
			function f(uint x) pure public {
				if (x > 7)
					revert;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "No matching declaration found");
}

BOOST_AUTO_TEST_CASE(revert_with_reason)
{
	char const* text = R"(
		contract C {
			function f(uint x) pure public {
				if (x > 7)
					revert("abc");
				else
					revert();
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(bare_others)
{
	CHECK_WARNING("contract C { function f() pure public { selfdestruct; } }", "Statement has no effect.");
	CHECK_WARNING("contract C { function f() pure public { assert; } }", "Statement has no effect.");
	// This is different because it does have overloads.
	CHECK_ERROR("contract C { function f() pure public { require; } }", TypeError, "No matching declaration found after variable lookup.");
	CHECK_WARNING("contract C { function f() pure public { suicide; } }", "Statement has no effect.");
}

BOOST_AUTO_TEST_CASE(pure_statement_in_for_loop)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				for (uint x = 0; x < 10; true)
					x++;
			}
		}
	)";
	CHECK_WARNING(text, "Statement has no effect.");
}

BOOST_AUTO_TEST_CASE(pure_statement_check_for_regular_for_loop)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				for (uint x = 0; true; x++)
				{}
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(warn_multiple_storage_storage_copies)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; }
			S x; S y;
			function f() public {
				(x, y) = (y, x);
			}
		}
	)";
	CHECK_WARNING(text, "This assignment performs two copies to storage.");
}

BOOST_AUTO_TEST_CASE(warn_multiple_storage_storage_copies_fill_right)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; }
			S x; S y;
			function f() public {
				(x, y, ) = (y, x, 1, 2);
			}
		}
	)";
	CHECK_WARNING(text, "This assignment performs two copies to storage.");
}

BOOST_AUTO_TEST_CASE(warn_multiple_storage_storage_copies_fill_left)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; }
			S x; S y;
			function f() public {
				(,x, y) = (1, 2, y, x);
			}
		}
	)";
	CHECK_WARNING(text, "This assignment performs two copies to storage.");
}

BOOST_AUTO_TEST_CASE(nowarn_swap_memory)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; }
			function f() pure public {
				S memory x;
				S memory y;
				(x, y) = (y, x);
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(nowarn_swap_storage_pointers)
{
	char const* text = R"(
		contract C {
			struct S { uint a; uint b; }
			S x; S y;
			function f() public {
				S storage x_local = x;
				S storage y_local = y;
				S storage z_local = x;
				(x, y_local, x_local, z_local) = (y, x_local, y_local, y);
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(warn_unused_local)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				uint a;
			}
		}
	)";
	CHECK_WARNING(text, "Unused local variable.");
}

BOOST_AUTO_TEST_CASE(warn_unused_local_assigned)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				uint a = 1;
			}
		}
	)";
	CHECK_WARNING(text, "Unused local variable.");
}

BOOST_AUTO_TEST_CASE(warn_unused_function_parameter)
{
	char const* text = R"(
		contract C {
			function f(uint a) pure public {
			}
		}
	)";
	CHECK_WARNING(text, "Unused function parameter. Remove or comment out the variable name to silence this warning.");
	text = R"(
		contract C {
			function f(uint a) pure public {
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(warn_unused_return_parameter)
{
	char const* text = R"(
		contract C {
			function f() pure public returns (uint a) {
			}
		}
	)";
	CHECK_WARNING(text, "Unused function parameter. Remove or comment out the variable name to silence this warning.");
	text = R"(
		contract C {
			function f() pure public returns (uint a) {
				return;
			}
		}
	)";
	CHECK_WARNING(text, "Unused function parameter. Remove or comment out the variable name to silence this warning.");
	text = R"(
		contract C {
			function f() pure public returns (uint) {
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
	text = R"(
		contract C {
			function f() pure public returns (uint a) {
				a = 1;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
	text = R"(
		contract C {
			function f() pure public returns (uint a) {
				return 1;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(no_unused_warning_interface_arguments)
{
	char const* text = R"(
		interface I {
			function f(uint a) pure external returns (uint b);
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(no_unused_warning_abstract_arguments)
{
	char const* text = R"(
		contract C {
			function f(uint a) pure public returns (uint b);
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(no_unused_warnings)
{
	char const* text = R"(
		contract C {
			function f(uint a) pure public returns (uint b) {
				uint c = 1;
				b = a + c;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(no_unused_dec_after_use)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				a = 7;
				uint a;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(no_unused_inline_asm)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				uint a;
				assembly {
					a := 1
				}
			}
		}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_with_functions)
{
	char const* text = R"(
		contract C {
			function keccak256() pure public {}
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_with_variables)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				uint msg;
				msg;
			}
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_with_storage_variables)
{
	char const* text = R"(
		contract C {
			uint msg;
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtin_at_global_scope)
{
	char const* text = R"(
		contract msg {
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_with_parameters)
{
	char const* text = R"(
		contract C {
			function f(uint require) pure public {
				require = 2;
			}
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_with_return_parameters)
{
	char const* text = R"(
		contract C {
			function f() pure public returns (uint require) {
				require = 2;
			}
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_with_events)
{
	char const* text = R"(
		contract C {
			event keccak256();
		}
	)";
	CHECK_WARNING(text, "shadows a builtin symbol");
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_ignores_struct)
{
	char const* text = R"(
		contract C {
			struct a {
				uint msg;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(shadowing_builtins_ignores_constructor)
{
	char const* text = R"(
		contract C {
			constructor() public {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(function_overload_is_not_shadowing)
{
	char const* text = R"(
		contract C {
			function f() pure public {}
			function f(uint) pure public {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(function_override_is_not_shadowing)
{
	char const* text = R"(
		contract D { function f() pure public {} }
		contract C is D {
			function f(uint) pure public {}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(event_parameter_cannot_shadow_state_variable)
{
	char const* text = R"(
		contract C {
			address a;
			event E(address a);
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(callable_crash)
{
	char const* text = R"(
		contract C {
			struct S { uint a; bool x; }
			S public s;
			function C() public {
				3({a: 1, x: true});
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type is not callable");
}

BOOST_AUTO_TEST_CASE(error_transfer_non_payable_fallback)
{
	// This used to be a test for a.transfer to generate a warning
	// because A's fallback function is not payable.
	char const* text = R"(
		contract A {
			function() public {}
		}

		contract B {
			A a;

			function() public {
				a.transfer(100);
			}
		}
	)";
	CHECK_ALLOW_MULTI(text, (std::vector<std::pair<Error::Type, std::string>>{
		{Error::Type::Warning, "Using contract member \"transfer\" inherited from the address type is deprecated"},
		{Error::Type::TypeError, "Value transfer to a contract without a payable fallback function"}
	}));
}

BOOST_AUTO_TEST_CASE(error_transfer_no_fallback)
{
	// This used to be a test for a.transfer to generate a warning
	// because A does not have a payable fallback function.
	std::string text = R"(
		contract A {}

		contract B {
			A a;

			function() public {
				a.transfer(100);
			}
		}
	)";
	CHECK_ALLOW_MULTI(text, (std::vector<std::pair<Error::Type, std::string>>{
		{Error::Type::Warning, "Using contract member \"transfer\" inherited from the address type is deprecated"},
		{Error::Type::TypeError, "Value transfer to a contract without a payable fallback function"}
	}));
}

BOOST_AUTO_TEST_CASE(error_send_non_payable_fallback)
{
	// This used to be a test for a.send to generate a warning
	// because A does not have a payable fallback function.
	std::string text = R"(
		contract A {
			function() public {}
		}

		contract B {
			A a;

			function() public {
				require(a.send(100));
			}
		}
	)";
	CHECK_ALLOW_MULTI(text, (std::vector<std::pair<Error::Type, std::string>>{
		{Error::Type::Warning, "Using contract member \"send\" inherited from the address type is deprecated"},
		{Error::Type::TypeError, "Value transfer to a contract without a payable fallback function"}
	}));
}

BOOST_AUTO_TEST_CASE(does_not_error_transfer_payable_fallback)
{
	// This used to be a test for a.transfer to generate a warning
	// because A does not have a payable fallback function.
	char const* text = R"(
		contract A {
			function() payable public {}
		}

		contract B {
			A a;

			function() public {
				a.transfer(100);
			}
		}
	)";
	CHECK_WARNING(text, "Using contract member \"transfer\" inherited from the address type is deprecated.");
}

BOOST_AUTO_TEST_CASE(does_not_error_transfer_regular_function)
{
	char const* text = R"(
		contract A {
			function transfer() pure public {}
		}

		contract B {
			A a;

			function() public {
				a.transfer();
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(returndatasize_as_variable)
{
	char const* text = R"(
		contract c { function f() public { uint returndatasize; assembly { returndatasize }}}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(create2_as_variable)
{
	char const* text = R"(
		contract c { function f() public { uint create2; assembly { create2(0, 0, 0, 0) } }}
	)";
	std::string msg = "Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md";
	CHECK_ERROR(text, SyntaxError, msg);
}

BOOST_AUTO_TEST_CASE(warn_unspecified_storage)
{
	char const* text = R"(
		contract C {
			struct S { uint a; string b; }
			S x;
			function f() view public {
				S storage y = x;
				y;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
	text = R"(
		contract C {
			struct S { uint a; }
			S x;
			function f() view public {
				S y = x;
				y;
			}
		}
	)";
	CHECK_WARNING(text, "Variable is declared as a storage pointer. Use an explicit \"storage\" keyword to silence this warning");
	text = R"(
		pragma experimental "v0.5.0";
		contract C {
			struct S { uint a; }
			S x;
			function f() view public {
				S y = x;
				y;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Storage location must be specified as either \"memory\" or \"storage\".");
}

BOOST_AUTO_TEST_CASE(storage_location_non_array_or_struct_disallowed)
{
	char const* text = R"(
		contract C {
			function f(uint storage a) public { }
		}
	)";
	CHECK_ERROR(text, TypeError, "Storage location can only be given for array or struct types.");
}

BOOST_AUTO_TEST_CASE(storage_location_non_array_or_struct_disallowed_is_not_fatal)
{
	char const* text = R"(
		contract C {
			function f(uint storage a) public {
				a = f;
			}
		}
	)";
	CHECK_ERROR_ALLOW_MULTI(text, TypeError, (std::vector<std::string>{"Storage location can only be given for array or struct types."}));
}

BOOST_AUTO_TEST_CASE(implicit_conversion_disallowed)
{
	char const* text = R"(
		contract C {
			function f() public returns (bytes4) {
				uint32 tmp = 1;
				return tmp;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Return argument type uint32 is not implicitly convertible to expected type (type of first return variable) bytes4.");
}

BOOST_AUTO_TEST_CASE(too_large_arrays_for_calldata)
{
	char const* text = R"(
		contract C {
			function f(uint[85678901234] a) pure external {
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Array is too large to be encoded.");
	text = R"(
		contract C {
			function f(uint[85678901234] a) pure internal {
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Array is too large to be encoded.");
	text = R"(
		contract C {
			function f(uint[85678901234] a) pure public {
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Array is too large to be encoded.");
}

BOOST_AUTO_TEST_CASE(explicit_literal_to_storage_string)
{
	char const* text = R"(
		contract C {
			function f() pure public {
				string memory x = "abc";
				x;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
	text = R"(
		contract C {
			function f() pure public {
				string storage x = "abc";
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type literal_string \"abc\" is not implicitly convertible to expected type string storage pointer.");
	text = R"(
		contract C {
			function f() pure public {
				string x = "abc";
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Type literal_string \"abc\" is not implicitly convertible to expected type string storage pointer.");
	text = R"(
		contract C {
			function f() pure public {
				string("abc");
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Explicit type conversion not allowed from \"literal_string \"abc\"\" to \"string storage pointer\"");
}

BOOST_AUTO_TEST_CASE(modifiers_access_storage_pointer)
{
	char const* text = R"(
		contract C {
			struct S { uint a; }
			modifier m(S storage x) {
				x;
				_;
			}
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(function_types_sig)
{
	char const* text = R"(
		contract C {
			function f() view returns (bytes4) {
				return f.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" not found");
	text = R"(
		contract C {
			function g() pure internal {
			}
			function f() view returns (bytes4) {
				return g.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" not found");
	text = R"(
		contract C {
			function f() view returns (bytes4) {
				function () g;
				return g.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" not found");
	text = R"(
		contract C {
			function f() pure external returns (bytes4) {
				return this.f.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
	text = R"(
		contract C {
			function h() pure external {
			}
			function f() view external returns (bytes4) {
				var g = this.h;
				return g.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
	text = R"(
		contract C {
			function h() pure external {
			}
			function f() view external returns (bytes4) {
				function () pure external g = this.h;
				return g.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
	text = R"(
		contract C {
			function h() pure external {
			}
			function f() view external returns (bytes4) {
				function () pure external g = this.h;
				var i = g;
				return i.selector;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"selector\" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
}

BOOST_AUTO_TEST_CASE(using_this_in_constructor)
{
	char const* text = R"(
		contract C {
			constructor() public {
				this.f();
			}
			function f() pure public {
			}
		}
	)";
	CHECK_WARNING(text, "\"this\" used in constructor");
}

BOOST_AUTO_TEST_CASE(do_not_crash_on_not_lvalue)
{
	// This checks for a bug that caused a crash because of continued analysis.
	char const* text = R"(
		contract C {
			mapping (uint => uint) m;
			function f() public {
				m(1) = 2;
			}
		}
	)";
	CHECK_ERROR_ALLOW_MULTI(text, TypeError, (std::vector<std::string>{
		"is not callable",
		"Expression has to be an lvalue",
		"Type int_const 2 is not implicitly"
	}));
}

BOOST_AUTO_TEST_CASE(builtin_reject_gas)
{
	char const* text = R"(
		contract C {
			function f() public {
				keccak256.gas();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"gas\" not found or not visible after argument-dependent lookup");
	text = R"(
		contract C {
			function f() public {
				sha256.gas();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"gas\" not found or not visible after argument-dependent lookup");
	text = R"(
		contract C {
			function f() public {
				ripemd160.gas();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"gas\" not found or not visible after argument-dependent lookup");
	text = R"(
		contract C {
			function f() public {
				ecrecover.gas();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"gas\" not found or not visible after argument-dependent lookup");
}

BOOST_AUTO_TEST_CASE(gasleft)
{
	char const* text = R"(
		contract C {
			function f() public view returns (uint256 val) { return msg.gas; }
		}
	)";
	CHECK_WARNING(text, "\"msg.gas\" has been deprecated in favor of \"gasleft()\"");

	text = R"(
		contract C {
			function f() public view returns (uint256 val) { return gasleft(); }
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);

	text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function f() public returns (uint256 val) { return msg.gas; }
		}
	)";
	CHECK_ERROR(text, TypeError, "\"msg.gas\" has been deprecated in favor of \"gasleft()\"");
}

BOOST_AUTO_TEST_CASE(gasleft_shadowing)
{
	char const* text = R"(
		contract C {
			function gasleft() public pure returns (bytes32 val) { return "abc"; }
			function f() public pure returns (bytes32 val) { return gasleft(); }
		}
	)";
	CHECK_WARNING(text, "This declaration shadows a builtin symbol.");

	text = R"(
		contract C {
			uint gasleft;
			function f() public { gasleft = 42; }
		}
	)";
	CHECK_WARNING(text, "This declaration shadows a builtin symbol.");
}

BOOST_AUTO_TEST_CASE(builtin_reject_value)
{
	char const* text = R"(
		contract C {
			function f() public {
				keccak256.value();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup");
	text = R"(
		contract C {
			function f() public {
				sha256.value();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup");
	text = R"(
		contract C {
			function f() public {
				ripemd160.value();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup");
	text = R"(
		contract C {
			function f() public {
				ecrecover.value();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Member \"value\" not found or not visible after argument-dependent lookup");
}

BOOST_AUTO_TEST_CASE(large_storage_array_fine)
{
	char const* text = R"(
		contract C {
			uint[2**64 - 1] x;
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
}

BOOST_AUTO_TEST_CASE(large_storage_array_simple)
{
	char const* text = R"(
		contract C {
			uint[2**64] x;
		}
	)";
	CHECK_WARNING(text, "covers a large part of storage and thus makes collisions likely");
}

BOOST_AUTO_TEST_CASE(large_storage_arrays_combined)
{
	char const* text = R"(
		contract C {
			uint[200][200][2**30][][2**30] x;
		}
	)";
	CHECK_WARNING(text, "covers a large part of storage and thus makes collisions likely");
}

BOOST_AUTO_TEST_CASE(large_storage_arrays_struct)
{
	char const* text = R"(
		contract C {
			struct S { uint[2**30] x; uint[2**50] y; }
			S[2**20] x;
		}
	)";
	CHECK_WARNING(text, "covers a large part of storage and thus makes collisions likely");
}

BOOST_AUTO_TEST_CASE(large_storage_array_mapping)
{
	char const* text = R"(
		contract C {
			mapping(uint => uint[2**100]) x;
		}
	)";
	CHECK_WARNING(text, "covers a large part of storage and thus makes collisions likely");
}

BOOST_AUTO_TEST_CASE(library_function_without_implementation)
{
	char const* text = R"(
		library L {
			function f() public;
		}
	)";
	CHECK_ERROR(text, TypeError, "External library functions without an implementation are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md");
	text = R"(
		library L {
			function f() internal;
		}
	)";
	CHECK_ERROR(text, TypeError, "Internal library function must be implemented if declared.");
	text = R"(
		library L {
			function f() private;
		}
	)";
	CHECK_ERROR(text, TypeError, "Internal library function must be implemented if declared.");
}

BOOST_AUTO_TEST_CASE(using_for_with_non_library)
{
	// This tests a crash that was resolved by making the first error fatal.
	char const* text = R"(
		library L {
			struct S { uint d; }
			using S for S;
			function f(S _s) internal {
				_s.d = 1;
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Library name expected.");
}

BOOST_AUTO_TEST_CASE(experimental_pragma)
{
	char const* text = R"(
		pragma experimental;
	)";
	CHECK_ERROR(text, SyntaxError, "Experimental feature name is missing.");
	text = R"(
		pragma experimental 123;
	)";
	CHECK_ERROR(text, SyntaxError, "Unsupported experimental feature name.");
	text = R"(
		pragma experimental unsupportedName;
	)";
	CHECK_ERROR(text, SyntaxError, "Unsupported experimental feature name.");
	text = R"(
		pragma experimental "unsupportedName";
	)";
	CHECK_ERROR(text, SyntaxError, "Unsupported experimental feature name.");
	text = R"(
		pragma experimental "";
	)";
	CHECK_ERROR(text, SyntaxError, "Empty experimental feature name is invalid.");
	text = R"(
		pragma experimental unsupportedName unsupportedName;
	)";
	CHECK_ERROR(text, SyntaxError, "Stray arguments.");
	text = R"(
		pragma experimental __test;
	)";
	CHECK_WARNING(text, "Experimental features are turned on. Do not use experimental features on live deployments.");
	text = R"(
		pragma experimental __test;
		pragma experimental __test;
	)";
	CHECK_ERROR_ALLOW_MULTI(text, SyntaxError, (std::vector<std::string>{"Duplicate experimental feature name."}));
}

BOOST_AUTO_TEST_CASE(reject_interface_creation)
{
	char const* text = R"(
		interface I {}
		contract C {
			function f() public {
				new I();
			}
		}
	)";
	CHECK_ERROR(text, TypeError, "Cannot instantiate an interface.");
}

BOOST_AUTO_TEST_CASE(accept_library_creation)
{
	char const* text = R"(
		library L {}
		contract C {
			function f() public {
				new L();
			}
		}
	)";
	CHECK_SUCCESS(text);
}

BOOST_AUTO_TEST_CASE(reject_interface_constructors)
{
	char const* text = R"(
		interface I {}
		contract C is I(2) {}
	)";
	CHECK_ERROR(text, TypeError, "Wrong argument count for constructor call: 1 arguments given but expected 0.");
}

BOOST_AUTO_TEST_CASE(non_external_fallback)
{
	char const* text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function () external { }
		}
	text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function () internal { }
		}
	)";
	CHECK_ERROR(text, TypeError, "Fallback function must be defined as \"external\".");
	text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function () private { }
		}
	)";
	CHECK_ERROR(text, TypeError, "Fallback function must be defined as \"external\".");
	text = R"(
		pragma experimental "v0.5.0";
		contract C {
			function () public { }
=======
	char const* sourceCode = R"(
		abstract contract C {
			function f(uint) public virtual returns (string memory);
			function g() public {
				string memory x = this.f(2);
				// we can assign to x but it is not usable.
				bytes(x).length;
			}
>>>>>>> theirs
		}
	)";
	if (solidity::test::CommonOptions::get().evmVersion() == EVMVersion::homestead())
		CHECK_ERROR(sourceCode, TypeError, "Type inaccessible dynamic type is not implicitly convertible to expected type string memory.");
	else
		CHECK_SUCCESS_NO_WARNINGS(sourceCode);
}

BOOST_AUTO_TEST_CASE(warn_nonpresent_pragma)
{
	char const* text = R"(
		// SPDX-License-Identifier: GPL-3.0
		contract C {}
	)";
	auto sourceAndError = parseAnalyseAndReturnError(text, true, false);
	BOOST_REQUIRE(!sourceAndError.second.empty());
	BOOST_REQUIRE(!!sourceAndError.first);
	BOOST_CHECK(searchErrorMessage(*sourceAndError.second.front(), "Source file does not specify required compiler version!"));
}

BOOST_AUTO_TEST_CASE(returndatasize_as_variable)
{
	char const* text = R"(
		contract C { function f() public pure { uint returndatasize; returndatasize; assembly { pop(returndatasize()) }}}
	)";
	vector<pair<Error::Type, std::string>> expectations(vector<pair<Error::Type, std::string>>{
		{Error::Type::Warning, "Variable is shadowed in inline assembly by an instruction of the same name"}
	});
	if (!solidity::test::CommonOptions::get().evmVersion().supportsReturndata())
	{
		expectations.emplace_back(make_pair(Error::Type::TypeError, std::string("\"returndatasize\" instruction is only available for Byzantium-compatible VMs")));
		expectations.emplace_back(make_pair(Error::Type::TypeError, std::string("Expected expression to evaluate to one value, but got 0 values instead.")));
	}
	CHECK_ALLOW_MULTI(text, expectations);
}

BOOST_AUTO_TEST_CASE(create2_as_variable)
{
	char const* text = R"(
		contract c { function f() public { uint create2; create2; assembly { pop(create2(0, 0, 0, 0)) } }}
	)";
	// This needs special treatment, because the message mentions the EVM version,
	// so cannot be run via isoltest.
	vector<pair<Error::Type, std::string>> expectations(vector<pair<Error::Type, std::string>>{
		{Error::Type::Warning, "Variable is shadowed in inline assembly by an instruction of the same name"}
	});
	if (!solidity::test::CommonOptions::get().evmVersion().hasCreate2())
	{
		expectations.emplace_back(make_pair(Error::Type::TypeError, std::string("\"create2\" instruction is only available for Constantinople-compatible VMs")));
		expectations.emplace_back(make_pair(Error::Type::TypeError, std::string("Expected expression to evaluate to one value, but got 0 values instead.")));
	}
	CHECK_ALLOW_MULTI(text, expectations);
}

BOOST_AUTO_TEST_CASE(extcodehash_as_variable)
{
	char const* text = R"(
		contract c { function f() public view { uint extcodehash; extcodehash; assembly { pop(extcodehash(0)) } }}
	)";
	// This needs special treatment, because the message mentions the EVM version,
	// so cannot be run via isoltest.
	vector<pair<Error::Type, std::string>> expectations(vector<pair<Error::Type, std::string>>{
		{Error::Type::Warning, "Variable is shadowed in inline assembly by an instruction of the same name"}
	});
	if (!solidity::test::CommonOptions::get().evmVersion().hasExtCodeHash())
	{
		expectations.emplace_back(make_pair(Error::Type::TypeError, std::string("\"extcodehash\" instruction is only available for Constantinople-compatible VMs")));
		expectations.emplace_back(make_pair(Error::Type::TypeError, std::string("Expected expression to evaluate to one value, but got 0 values instead.")));
	}
	CHECK_ALLOW_MULTI(text, expectations);
}

BOOST_AUTO_TEST_CASE(getter_is_memory_type)
{
	char const* text = R"(
		contract C {
			struct S { string m; }
			string[] public x;
			S[] public y;
		}
	)";
	CHECK_SUCCESS_NO_WARNINGS(text);
	// Check that the getters return a memory strings, not a storage strings.
	ContractDefinition const& c = dynamic_cast<ContractDefinition const&>(*compiler().ast("").nodes().at(1));
	BOOST_CHECK(c.interfaceFunctions().size() == 2);
	for (auto const& f: c.interfaceFunctions())
	{
		auto const& retType = f.second->returnParameterTypes().at(0);
		BOOST_CHECK(retType->dataStoredIn(DataLocation::Memory));
	}
}

BOOST_AUTO_TEST_CASE(address_staticcall)
{
	char const* sourceCode = R"(
		contract C {
			function f() public view returns(bool) {
				(bool success,) = address(0x4242).staticcall("");
				return success;
			}
		}
	)";

	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
		CHECK_SUCCESS_NO_WARNINGS(sourceCode);
	else
		CHECK_ERROR(sourceCode, TypeError, "\"staticcall\" is not supported by the VM version.");
}

BOOST_AUTO_TEST_CASE(address_staticcall_value)
{
	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
	{
		char const* sourceCode = R"(
			contract C {
				function f() public view {
					address(0x4242).staticcall.value;
				}
			}
		)";
		CHECK_ERROR(sourceCode, TypeError, "Member \"value\" is only available for payable functions.");
	}
}

BOOST_AUTO_TEST_CASE(address_call_full_return_type)
{
	char const* sourceCode = R"(
		contract C {
			function f() public {
				(bool success, bytes memory m) = address(0x4242).call("");
				success; m;
			}
		}
	)";

	if (solidity::test::CommonOptions::get().evmVersion().supportsReturndata())
		CHECK_SUCCESS_NO_WARNINGS(sourceCode);
	else
		CHECK_ERROR(sourceCode, TypeError, "Type inaccessible dynamic type is not implicitly convertible to expected type bytes memory.");
}

BOOST_AUTO_TEST_CASE(address_delegatecall_full_return_type)
{
	char const* sourceCode = R"(
		contract C {
			function f() public {
				(bool success, bytes memory m) = address(0x4242).delegatecall("");
				success; m;
			}
		}
	)";

	if (solidity::test::CommonOptions::get().evmVersion().supportsReturndata())
		CHECK_SUCCESS_NO_WARNINGS(sourceCode);
	else
		CHECK_ERROR(sourceCode, TypeError, "Type inaccessible dynamic type is not implicitly convertible to expected type bytes memory.");
}


BOOST_AUTO_TEST_CASE(address_staticcall_full_return_type)
{
	if (solidity::test::CommonOptions::get().evmVersion().hasStaticCall())
	{
		char const* sourceCode = R"(
			contract C {
				function f() public view {
					(bool success, bytes memory m) = address(0x4242).staticcall("");
					success; m;
				}
			}
		)";

		CHECK_SUCCESS_NO_WARNINGS(sourceCode);
	}
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
