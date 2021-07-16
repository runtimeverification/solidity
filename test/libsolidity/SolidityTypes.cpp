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
 * @date 2015
 * Unit tests for the type system of Solidity.
 */

#include <libsolidity/ast/Types.h>
#include <libsolidity/ast/TypeProvider.h>
#include <libsolidity/ast/AST.h>
#include <libsolutil/Keccak256.h>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace solidity::langutil;

namespace solidity::frontend::test
{

BOOST_AUTO_TEST_SUITE(SolidityTypes)

BOOST_AUTO_TEST_CASE(int_types)
{
	BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::Int, 0, 0)) == *TypeProvider::integer(IntegerType::Modifier::Signed));
	for (unsigned i = 8; i <= 256; i += 8)
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::IntM, i, 0)) == *TypeProvider::integer(i, IntegerType::Modifier::Signed));
}

BOOST_AUTO_TEST_CASE(uint_types)
{
	BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::UInt, 0, 0)) == *TypeProvider::integer(IntegerType::Modifier::Unsigned));
	for (unsigned i = 8; i <= 256; i += 8)
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::UIntM, i, 0)) == *TypeProvider::integer(i, IntegerType::Modifier::Unsigned));
}

BOOST_AUTO_TEST_CASE(byte_types)
{
	for (unsigned i = 1; i <= 32; i++)
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::BytesM, i, 0)) == *TypeProvider::fixedBytes(i));
}

BOOST_AUTO_TEST_CASE(fixed_types)
{
	BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::Fixed, 0, 0)) == *TypeProvider::fixedPoint(128, 18, FixedPointType::Modifier::Signed));
	for (unsigned i = 8; i <= 256; i += 8)
	{
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::FixedMxN, i, 0)) == *TypeProvider::fixedPoint(i, 0, FixedPointType::Modifier::Signed));
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::FixedMxN, i, 2)) == *TypeProvider::fixedPoint(i, 2, FixedPointType::Modifier::Signed));
	}
}

BOOST_AUTO_TEST_CASE(ufixed_types)
{
	BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::UFixed, 0, 0)) == *TypeProvider::fixedPoint(128, 18, FixedPointType::Modifier::Unsigned));
	for (unsigned i = 8; i <= 256; i += 8)
	{
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::UFixedMxN, i, 0)) == *TypeProvider::fixedPoint(i, 0, FixedPointType::Modifier::Unsigned));
		BOOST_CHECK(*TypeProvider::fromElementaryTypeName(ElementaryTypeNameToken(Token::UFixedMxN, i, 2)) == *TypeProvider::fixedPoint(i, 2, FixedPointType::Modifier::Unsigned));
	}
}

BOOST_AUTO_TEST_CASE(storage_layout_simple)
{
	MemberList members(MemberList::MemberMap({
		{string("first"), TypeProvider::fromElementaryTypeName("uint128")},
		{string("second"), TypeProvider::fromElementaryTypeName("uint120")},
		{string("wraps"), TypeProvider::fromElementaryTypeName("uint16")}
	}));
	BOOST_REQUIRE_EQUAL(u256(3), members.storageSize());
	BOOST_REQUIRE(members.memberStorageOffset("first") != nullptr);
	BOOST_REQUIRE(members.memberStorageOffset("second") != nullptr);
	BOOST_REQUIRE(members.memberStorageOffset("wraps") != nullptr);
	BOOST_CHECK(*members.memberStorageOffset("first") == make_pair(bigint(0), unsigned(0)));
	BOOST_CHECK(*members.memberStorageOffset("second") == make_pair(bigint(1), unsigned(0)));
	BOOST_CHECK(*members.memberStorageOffset("wraps") == make_pair(bigint(2), unsigned(0)));
}

BOOST_AUTO_TEST_CASE(storage_layout_mapping)
{
	MemberList members(MemberList::MemberMap({
		{string("first"), TypeProvider::fromElementaryTypeName("uint128")},
		{string("second"), TypeProvider::mapping(
			TypeProvider::fromElementaryTypeName("uint8"),
			TypeProvider::fromElementaryTypeName("uint8")
		)},
		{string("third"), TypeProvider::fromElementaryTypeName("uint16")},
		{string("final"), TypeProvider::mapping(
			TypeProvider::fromElementaryTypeName("uint8"),
			TypeProvider::fromElementaryTypeName("uint8")
		)},
	}));
	BOOST_REQUIRE_EQUAL(bigint(514), members.storageSize());
	BOOST_REQUIRE(members.memberStorageOffset("first") != nullptr);
	BOOST_REQUIRE(members.memberStorageOffset("second") != nullptr);
	BOOST_REQUIRE(members.memberStorageOffset("third") != nullptr);
	BOOST_REQUIRE(members.memberStorageOffset("final") != nullptr);
	BOOST_CHECK(*members.memberStorageOffset("first") == make_pair(bigint(0), unsigned(0)));
	BOOST_CHECK(*members.memberStorageOffset("second") == make_pair(bigint(1), unsigned(0)));
	BOOST_CHECK(*members.memberStorageOffset("third") == make_pair(bigint(257), unsigned(0)));
	BOOST_CHECK(*members.memberStorageOffset("final") == make_pair(bigint(258), unsigned(0)));
}

BOOST_AUTO_TEST_CASE(storage_layout_arrays)
{
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(1), 32).storageSize() == 32);
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(1), 33).storageSize() == 33);
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(2), 31).storageSize() == 31);
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(7), 8).storageSize() == 8);
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(7), 9).storageSize() == 9);
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(31), 9).storageSize() == 9);
	BOOST_CHECK(ArrayType(DataLocation::Storage, TypeProvider::fixedBytes(32), 9).storageSize() == 9);
}

BOOST_AUTO_TEST_CASE(type_identifier_escaping)
{
	BOOST_CHECK_EQUAL(Type::escapeIdentifier("("), "$_");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier(")"), "_$");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier(","), "_$_");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier("$"), "$$$");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier(")$("), "_$$$$$_");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier("()"), "$__$");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier("(,)"), "$__$__$");
	BOOST_CHECK_EQUAL(Type::escapeIdentifier("(,$,)"), "$__$_$$$_$__$");
	BOOST_CHECK_EQUAL(
		Type::escapeIdentifier("((__(_$_$$,__($$,,,$$),$,,,)))$$,$$"),
		"$_$___$__$$$_$$$$$$_$___$_$$$$$$_$__$__$_$$$$$$_$_$_$$$_$__$__$__$_$_$$$$$$$_$_$$$$$$"
	);
}

BOOST_AUTO_TEST_CASE(type_identifiers)
{
	int64_t id = 0;

	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("uint128")->identifier(), "t_uint128");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("int128")->identifier(), "t_int128");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("address")->identifier(), "t_address");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("uint8")->identifier(), "t_uint8");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("ufixed64x2")->identifier(), "t_ufixed64x2");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("fixed128x8")->identifier(), "t_fixed128x8");
	BOOST_CHECK_EQUAL(RationalNumberType(rational(7, 1)).identifier(), "t_rational_7_by_1");
	BOOST_CHECK_EQUAL(RationalNumberType(rational(200, 77)).identifier(), "t_rational_200_by_77");
	BOOST_CHECK_EQUAL(RationalNumberType(rational(2 * 200, 2 * 77)).identifier(), "t_rational_200_by_77");
	BOOST_CHECK_EQUAL(RationalNumberType(rational(-2 * 200, 2 * 77)).identifier(), "t_rational_minus_200_by_77");
	BOOST_CHECK_EQUAL(
		StringLiteralType(Literal(++id, SourceLocation{}, Token::StringLiteral, make_shared<string>("abc - def"))).identifier(),
		 "t_stringliteral_196a9142ee0d40e274a6482393c762b16dd8315713207365e1e13d8d85b74fc4"
	);
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes1")->identifier(), "t_bytes1");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes8")->identifier(), "t_bytes8");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes32")->identifier(), "t_bytes32");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bool")->identifier(), "t_bool");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes")->identifier(), "t_bytes_storage_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes memory")->identifier(), "t_bytes_memory_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes storage")->identifier(), "t_bytes_storage_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("bytes calldata")->identifier(), "t_bytes_calldata_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("string")->identifier(), "t_string_storage_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("string memory")->identifier(), "t_string_memory_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("string storage")->identifier(), "t_string_storage_ptr");
	BOOST_CHECK_EQUAL(TypeProvider::fromElementaryTypeName("string calldata")->identifier(), "t_string_calldata_ptr");
	ArrayType largeintArray(DataLocation::Memory, TypeProvider::fromElementaryTypeName("int128"), u256("2535301200456458802993406410752"));
	BOOST_CHECK_EQUAL(largeintArray.identifier(), "t_array$_t_int128_$2535301200456458802993406410752_memory_ptr");
	TypePointer stringArray = TypeProvider::array(DataLocation::Storage, TypeProvider::fromElementaryTypeName("string"), u256("20"));
	TypePointer multiArray = TypeProvider::array(DataLocation::Storage, stringArray);
	BOOST_CHECK_EQUAL(multiArray->identifier(), "t_array$_t_array$_t_string_storage_$20_storage_$dyn_storage_ptr");

	ContractDefinition c(++id, SourceLocation{}, make_shared<string>("MyContract$"), {}, {}, {}, ContractKind::Contract);
	BOOST_CHECK_EQUAL(c.type()->identifier(), "t_type$_t_contract$_MyContract$$$_$2_$");
	BOOST_CHECK_EQUAL(ContractType(c, true).identifier(), "t_super$_MyContract$$$_$2");

	StructDefinition s(++id, {}, make_shared<string>("Struct"), {});
	s.annotation().recursive = false;
	BOOST_CHECK_EQUAL(s.type()->identifier(), "t_type$_t_struct$_Struct_$3_storage_ptr_$");

	EnumDefinition e(++id, {}, make_shared<string>("Enum"), {});
	BOOST_CHECK_EQUAL(e.type()->identifier(), "t_type$_t_enum$_Enum_$4_$");

	TupleType t({e.type(), s.type(), stringArray, nullptr});
	BOOST_CHECK_EQUAL(t.identifier(), "t_tuple$_t_type$_t_enum$_Enum_$4_$_$_t_type$_t_struct$_Struct_$3_storage_ptr_$_$_t_array$_t_string_storage_$20_storage_ptr_$__$");

	TypePointer keccak256fun = TypeProvider::function(strings{}, strings{}, FunctionType::Kind::KECCAK256);
	BOOST_CHECK_EQUAL(keccak256fun->identifier(), "t_function_keccak256_nonpayable$__$returns$__$");

	FunctionType metaFun(TypePointers{keccak256fun}, TypePointers{s.type()}, strings{""}, strings{""});
	BOOST_CHECK_EQUAL(metaFun.identifier(), "t_function_internal_nonpayable$_t_function_keccak256_nonpayable$__$returns$__$_$returns$_t_type$_t_struct$_Struct_$3_storage_ptr_$_$");

	TypePointer m = TypeProvider::mapping(TypeProvider::fromElementaryTypeName("bytes32"), s.type());
	MappingType m2(TypeProvider::fromElementaryTypeName("uint64"), m);
	BOOST_CHECK_EQUAL(m2.identifier(), "t_mapping$_t_uint64_$_t_mapping$_t_bytes32_$_t_type$_t_struct$_Struct_$3_storage_ptr_$_$_$");

	// TypeType is tested with contract

	auto emptyParams = make_shared<ParameterList>(++id, SourceLocation(), std::vector<ASTPointer<VariableDeclaration>>());
	ModifierDefinition mod(++id, SourceLocation{}, make_shared<string>("modif"), {}, emptyParams, {}, {}, {});
	BOOST_CHECK_EQUAL(ModifierType(mod).identifier(), "t_modifier$__$");

	SourceUnit su(++id, {}, {}, {});
	BOOST_CHECK_EQUAL(ModuleType(su).identifier(), "t_module_7");
	BOOST_CHECK_EQUAL(MagicType(MagicType::Kind::Block).identifier(), "t_magic_block");
	BOOST_CHECK_EQUAL(MagicType(MagicType::Kind::Message).identifier(), "t_magic_message");
	BOOST_CHECK_EQUAL(MagicType(MagicType::Kind::Transaction).identifier(), "t_magic_transaction");

	BOOST_CHECK_EQUAL(InaccessibleDynamicType().identifier(), "t_inaccessible");
}

BOOST_AUTO_TEST_CASE(encoded_sizes)
{
	BOOST_CHECK_EQUAL(IntegerType(16).calldataEncodedSize(true), 32);
	BOOST_CHECK_EQUAL(IntegerType(16).calldataEncodedSize(false), 2);

	BOOST_CHECK_EQUAL(FixedBytesType(16).calldataEncodedSize(true), 32);
	BOOST_CHECK_EQUAL(FixedBytesType(16).calldataEncodedSize(false), 16);

	BOOST_CHECK_EQUAL(BoolType().calldataEncodedSize(true), 32);
	BOOST_CHECK_EQUAL(BoolType().calldataEncodedSize(false), 1);

	ArrayType const* uint24Array = TypeProvider::array(
		DataLocation::Memory,
		TypeProvider::uint(24),
		9
	);
	BOOST_CHECK_EQUAL(uint24Array->calldataEncodedSize(true), 9 * 32);
	BOOST_CHECK_EQUAL(uint24Array->calldataEncodedSize(false), 9 * 32);

	ArrayType twoDimArray(DataLocation::Memory, uint24Array, 3);
	BOOST_CHECK_EQUAL(twoDimArray.calldataEncodedSize(true),  9 * 3 * 32);
	BOOST_CHECK_EQUAL(twoDimArray.calldataEncodedSize(false), 9 * 3 * 32);
}

BOOST_AUTO_TEST_CASE(helper_bool_result)
{
	BoolResult r1{true};
	BoolResult r2 = BoolResult::err("Failure.");
	r1.merge(r2, logical_and<bool>());
	BOOST_REQUIRE_EQUAL(r1.get(), false);
	BOOST_REQUIRE_EQUAL(r1.message(), "Failure.");

	BoolResult r3{false};
	BoolResult r4{true};
	r3.merge(r4, logical_and<bool>());
	BOOST_REQUIRE_EQUAL(r3.get(), false);
	BOOST_REQUIRE_EQUAL(r3.message(), "");

	BoolResult r5{true};
	BoolResult r6{true};
	r5.merge(r6, logical_and<bool>());
	BOOST_REQUIRE_EQUAL(r5.get(), true);
	BOOST_REQUIRE_EQUAL(r5.message(), "");
}

BOOST_AUTO_TEST_CASE(helper_string_result)
{
	using StringResult = util::Result<string>;

	StringResult r1{string{"Success"}};
	StringResult r2 = StringResult::err("Failure");

	BOOST_REQUIRE_EQUAL(r1.get(), "Success");
	BOOST_REQUIRE_EQUAL(r2.get(), "");

	r1.merge(r2, [](string const&, string const& _rhs) { return _rhs; });

	BOOST_REQUIRE_EQUAL(r1.get(), "");
	BOOST_REQUIRE_EQUAL(r1.message(), "Failure");
}

BOOST_AUTO_TEST_SUITE_END()

}
