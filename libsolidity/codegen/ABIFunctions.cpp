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
 * @author Christian <chris@ethereum.org>
 * @date 2017
 * Routines that generate JULIA code related to ABI encoding, decoding and type conversions.
 */

#include <libsolidity/codegen/ABIFunctions.h>

#include <libsolidity/ast/AST.h>
#include <libsolidity/codegen/CompilerUtils.h>

#include <libdevcore/Whiskers.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/reversed.hpp>

using namespace std;
using namespace dev;
using namespace dev::solidity;

string ABIFunctions::tupleEncoder(
	TypePointers const& _givenTypes,
	TypePointers const& _targetTypes,
	bool _encodeAsLibraryTypes
)
{
	string functionName = string("abi_encode_tuple_");
	for (auto const& t: _givenTypes)
		functionName += t->identifier() + "_";
	functionName += "_to_";
	for (auto const& t: _targetTypes)
		functionName += t->identifier() + "_";
	if (_encodeAsLibraryTypes)
		functionName += "_library";

	return createFunction(functionName, [&]() {
		solAssert(!_givenTypes.empty(), "");

		// Note that the values are in reverse due to the difference in calling semantics.
		Whiskers templ(R"(
			function <functionName>(headStart <valueParams>) -> tail {
				tail := add(headStart, <headSize>)
				<encodeElements>
			}
		)");
		templ("functionName", functionName);
		size_t const headSize_ = headSize(_targetTypes);
		templ("headSize", to_string(headSize_));
		string valueParams;
		string encodeElements;
		size_t headPos = 0;
		size_t stackPos = 0;
		for (size_t i = 0; i < _givenTypes.size(); ++i)
		{
			solAssert(_givenTypes[i], "");
			solAssert(_targetTypes[i], "");
			size_t sizeOnStack = _givenTypes[i]->sizeOnStack();
			string valueNames = "";
			for (size_t j = 0; j < sizeOnStack; j++)
			{
				valueNames += "value" + to_string(stackPos) + ", ";
				valueParams = ", value" + to_string(stackPos) + valueParams;
				stackPos++;
			}
			bool dynamic = _targetTypes[i]->isDynamicallyEncoded();
			Whiskers elementTempl(
				dynamic ?
				string(R"(
					mstore(add(headStart, <pos>), sub(tail, headStart))
					tail := <abiEncode>(<values> tail)
				)") :
				string(R"(
					<abiEncode>(<values> add(headStart, <pos>))
				)")
			);
			elementTempl("values", valueNames);
			elementTempl("pos", to_string(headPos));
			elementTempl("abiEncode", abiEncodingFunction(*_givenTypes[i], *_targetTypes[i], _encodeAsLibraryTypes, true));
			encodeElements += elementTempl.render();
			headPos += dynamic ? 0x20 : _targetTypes[i]->calldataEncodedSize();
		}
		solAssert(headPos == headSize_, "");
		templ("valueParams", valueParams);
		templ("encodeElements", encodeElements);

		return templ.render();
	});
}

string ABIFunctions::tupleDecoder(TypePointers const& _types, bool _fromMemory)
{
	string functionName = string("abi_decode_tuple_");
	for (auto const& t: _types)
		functionName += t->identifier();
	if (_fromMemory)
		functionName += "_fromMemory";

	solAssert(!_types.empty(), "");

	return createFunction(functionName, [&]() {
		TypePointers decodingTypes;
		for (auto const& t: _types)
			decodingTypes.emplace_back(t->decodingType());

		Whiskers templ(R"(
			function <functionName>(headStart, dataEnd) -> <valueReturnParams> {
				if slt(sub(dataEnd, headStart), <minimumSize>) { revert(0, 0) }
				<decodeElements>
			}
		)");
		templ("functionName", functionName);
		templ("minimumSize", to_string(headSize(decodingTypes)));

		string decodeElements;
		vector<string> valueReturnParams;
		size_t headPos = 0;
		size_t stackPos = 0;
		for (size_t i = 0; i < _types.size(); ++i)
		{
			solAssert(_types[i], "");
			solAssert(decodingTypes[i], "");
			size_t sizeOnStack = _types[i]->sizeOnStack();
			solAssert(sizeOnStack == decodingTypes[i]->sizeOnStack(), "");
			solAssert(sizeOnStack > 0, "");
			vector<string> valueNamesLocal;
			for (size_t j = 0; j < sizeOnStack; j++)
			{
				valueNamesLocal.push_back("value" + to_string(stackPos));
				valueReturnParams.push_back("value" + to_string(stackPos));
				stackPos++;
			}
			bool dynamic = decodingTypes[i]->isDynamicallyEncoded();
			Whiskers elementTempl(
				dynamic ?
				R"(
				{
					let offset := <load>(add(headStart, <pos>))
					if gt(offset, 0xffffffffffffffff) { revert(0, 0) }
					<values> := <abiDecode>(add(headStart, offset), dataEnd)
				}
				)" :
				R"(
				{
					let offset := <pos>
					<values> := <abiDecode>(add(headStart, offset), dataEnd)
				}
				)"
			);
			elementTempl("load", _fromMemory ? "mload" : "calldataload");
			elementTempl("values", boost::algorithm::join(valueNamesLocal, ", "));
			elementTempl("pos", to_string(headPos));
			elementTempl("abiDecode", abiDecodingFunction(*_types[i], _fromMemory, true));
			decodeElements += elementTempl.render();
			headPos += dynamic ? 0x20 : decodingTypes[i]->calldataEncodedSize();
		}
		templ("valueReturnParams", boost::algorithm::join(valueReturnParams, ", "));
		templ("decodeElements", decodeElements);

		return templ.render();
	});
}

string ABIFunctions::requestedFunctions()
{
	string result;
	for (auto const& f: m_requestedFunctions)
		result += f.second;
	m_requestedFunctions.clear();
	return result;
}

string ABIFunctions::cleanupFunction(Type const& _type, bool _revertOnFailure)
{
	string functionName = string("cleanup_") + (_revertOnFailure ? "revert_" : "assert_") + _type.identifier();
	return createFunction(functionName, [&]() {
		Whiskers templ(R"(
			function <functionName>(value) -> cleaned {
				<body>
			}
		)");
		templ("functionName", functionName);
		switch (_type.category())
		{
		case Type::Category::Integer:
		{
			IntegerType const& type = dynamic_cast<IntegerType const&>(_type);
			if (type.numBits() == 256)
				templ("body", "cleaned := value");
			else if (type.isSigned())
				templ("body", "cleaned := signextend(" + to_string(type.numBits() / 8 - 1) + ", value)");
			else
				templ("body", "cleaned := and(value, " + toCompactHexWithPrefix((u256(1) << type.numBits()) - 1) + ")");
			break;
		}
		case Type::Category::RationalNumber:
			templ("body", "cleaned := value");
			break;
		case Type::Category::Bool:
			templ("body", "cleaned := iszero(iszero(value))");
			break;
		case Type::Category::FixedPoint:
			solUnimplemented("Fixed point types not implemented.");
			break;
		case Type::Category::Array:
		case Type::Category::Struct:
			solAssert(_type.dataStoredIn(DataLocation::Storage), "Cleanup requested for non-storage reference type.");
			templ("body", "cleaned := value");
			break;
		case Type::Category::FixedBytes:
		{
			FixedBytesType const& type = dynamic_cast<FixedBytesType const&>(_type);
			if (type.numBytes() == 32)
				templ("body", "cleaned := value");
			else if (type.numBytes() == 0)
				templ("body", "cleaned := 0");
			else
			{
				size_t numBits = type.numBytes() * 8;
				u256 mask = ((u256(1) << numBits) - 1) << (256 - numBits);
				templ("body", "cleaned := and(value, " + toCompactHexWithPrefix(mask) + ")");
			}
			break;
		}
		case Type::Category::Contract:
			templ("body", "cleaned := " + cleanupFunction(IntegerType(160, IntegerType::Modifier::Address)) + "(value)");
			break;
		case Type::Category::Enum:
		{
			size_t members = dynamic_cast<EnumType const&>(_type).numberOfMembers();
			solAssert(members > 0, "empty enum should have caused a parser error.");
			Whiskers w("if iszero(lt(value, <members>)) { <failure> } cleaned := value");
			w("members", to_string(members));
			if (_revertOnFailure)
				w("failure", "revert(0, 0)");
			else
				w("failure", "invalid()");
			templ("body", w.render());
			break;
		}
		case Type::Category::InaccessibleDynamic:
			templ("body", "cleaned := 0");
			break;
		default:
			solAssert(false, "Cleanup of type " + _type.identifier() + " requested.");
		}

		return templ.render();
	});
}

string ABIFunctions::conversionFunction(Type const& _from, Type const& _to)
{
	string functionName =
		"convert_" +
		_from.identifier() +
		"_to_" +
		_to.identifier();
	return createFunction(functionName, [&]() {
		Whiskers templ(R"(
			function <functionName>(value) -> converted {
				<body>
			}
		)");
		templ("functionName", functionName);
		string body;
		auto toCategory = _to.category();
		auto fromCategory = _from.category();
		switch (fromCategory)
		{
		case Type::Category::Integer:
		case Type::Category::RationalNumber:
		case Type::Category::Contract:
		{
			if (RationalNumberType const* rational = dynamic_cast<RationalNumberType const*>(&_from))
				solUnimplementedAssert(!rational->isFractional(), "Not yet implemented - FixedPointType.");
			if (toCategory == Type::Category::FixedBytes)
			{
				solAssert(
					fromCategory == Type::Category::Integer || fromCategory == Type::Category::RationalNumber,
					"Invalid conversion to FixedBytesType requested."
				);
				FixedBytesType const& toBytesType = dynamic_cast<FixedBytesType const&>(_to);
				body =
					Whiskers("converted := <shiftLeft>(<clean>(value))")
						("shiftLeft", shiftLeftFunction(256 - toBytesType.numBytes() * 8))
						("clean", cleanupFunction(_from))
						.render();
			}
			else if (toCategory == Type::Category::Enum)
			{
				solAssert(_from.mobileType(), "");
				body =
					Whiskers("converted := <cleanEnum>(<cleanInt>(value))")
					("cleanEnum", cleanupFunction(_to, false))
					// "mobileType()" returns integer type for rational
					("cleanInt", cleanupFunction(*_from.mobileType()))
					.render();
			}
			else if (toCategory == Type::Category::FixedPoint)
			{
				solUnimplemented("Not yet implemented - FixedPointType.");
			}
			else
			{
				solAssert(
					toCategory == Type::Category::Integer ||
					toCategory == Type::Category::Contract,
				"");
				IntegerType const addressType(160, IntegerType::Modifier::Address);
				IntegerType const& to =
					toCategory == Type::Category::Integer ?
					dynamic_cast<IntegerType const&>(_to) :
					addressType;

				// Clean according to the "to" type, except if this is
				// a widening conversion.
				IntegerType const* cleanupType = &to;
				if (fromCategory != Type::Category::RationalNumber)
				{
					IntegerType const& from =
						fromCategory == Type::Category::Integer ?
						dynamic_cast<IntegerType const&>(_from) :
						addressType;
					if (to.numBits() > from.numBits())
						cleanupType = &from;
				}
				body =
					Whiskers("converted := <cleanInt>(value)")
					("cleanInt", cleanupFunction(*cleanupType))
					.render();
			}
			break;
		}
		case Type::Category::Bool:
		{
			solAssert(_from == _to, "Invalid conversion for bool.");
			body =
				Whiskers("converted := <clean>(value)")
				("clean", cleanupFunction(_from))
				.render();
			break;
		}
		case Type::Category::FixedPoint:
			solUnimplemented("Fixed point types not implemented.");
			break;
		case Type::Category::Array:
			solUnimplementedAssert(false, "Array conversion not implemented.");
			break;
		case Type::Category::Struct:
			solUnimplementedAssert(false, "Struct conversion not implemented.");
			break;
		case Type::Category::FixedBytes:
		{
			FixedBytesType const& from = dynamic_cast<FixedBytesType const&>(_from);
			if (toCategory == Type::Category::Integer)
				body =
					Whiskers("converted := <convert>(<shift>(value))")
					("shift", shiftRightFunction(256 - from.numBytes() * 8, false))
					("convert", conversionFunction(IntegerType(from.numBytes() * 8), _to))
					.render();
			else
			{
				// clear for conversion to longer bytes
				solAssert(toCategory == Type::Category::FixedBytes, "Invalid type conversion requested.");
				body =
					Whiskers("converted := <clean>(value)")
					("clean", cleanupFunction(from))
					.render();
			}
			break;
		}
		case Type::Category::Function:
		{
			solAssert(false, "Conversion should not be called for function types.");
			break;
		}
		case Type::Category::Enum:
		{
			solAssert(toCategory == Type::Category::Integer || _from == _to, "");
			EnumType const& enumType = dynamic_cast<decltype(enumType)>(_from);
			body =
				Whiskers("converted := <clean>(value)")
				("clean", cleanupFunction(enumType))
				.render();
			break;
		}
		case Type::Category::Tuple:
		{
			solUnimplementedAssert(false, "Tuple conversion not implemented.");
			break;
		}
		default:
			solAssert(false, "");
		}

		solAssert(!body.empty(), "");
		templ("body", body);
		return templ.render();
	});
}

string ABIFunctions::cleanupCombinedExternalFunctionIdFunction()
{
	string functionName = "cleanup_combined_external_function_id";
	return createFunction(functionName, [&]() {
		return Whiskers(R"(
			function <functionName>(addr_and_selector) -> cleaned {
				cleaned := <clean>(addr_and_selector)
			}
		)")
		("functionName", functionName)
		("clean", cleanupFunction(FixedBytesType(24)))
		.render();
	});
}

string ABIFunctions::combineExternalFunctionIdFunction()
{
	string functionName = "combine_external_function_id";
	return createFunction(functionName, [&]() {
		return Whiskers(R"(
			function <functionName>(addr, selector) -> combined {
				combined := <shl64>(or(<shl32>(addr), and(selector, 0xffffffff)))
			}
		)")
		("functionName", functionName)
		("shl32", shiftLeftFunction(32))
		("shl64", shiftLeftFunction(64))
		.render();
	});
}

string ABIFunctions::splitExternalFunctionIdFunction()
{
	string functionName = "split_external_function_id";
	return createFunction(functionName, [&]() {
		return Whiskers(R"(
			function <functionName>(combined) -> addr, selector {
				combined := <shr64>(combined)
				selector := and(combined, 0xffffffff)
				addr := <shr32>(combined)
			}
		)")
		("functionName", functionName)
		("shr32", shiftRightFunction(32, false))
		("shr64", shiftRightFunction(64, false))
		.render();
	});
}

string ABIFunctions::abiEncodingFunction(
	Type const& _from,
	Type const& _to,
	bool _encodeAsLibraryTypes,
	bool _fromStack
)
{
	solUnimplementedAssert(
		_to.mobileType() &&
		_to.mobileType()->interfaceType(_encodeAsLibraryTypes) &&
		_to.mobileType()->interfaceType(_encodeAsLibraryTypes)->encodingType(),
		"Encoding type \"" + _to.toString() + "\" not yet implemented."
	);
	TypePointer toInterface = _to.mobileType()->interfaceType(_encodeAsLibraryTypes)->encodingType();
	Type const& to = *toInterface;

	if (_from.category() == Type::Category::StringLiteral)
		return abiEncodingFunctionStringLiteral(_from, to, _encodeAsLibraryTypes);
	else if (auto toArray = dynamic_cast<ArrayType const*>(&to))
	{
		solAssert(_from.category() == Type::Category::Array, "");
		solAssert(to.dataStoredIn(DataLocation::Memory), "");
		ArrayType const& fromArray = dynamic_cast<ArrayType const&>(_from);
		if (fromArray.location() == DataLocation::CallData)
			return abiEncodingFunctionCalldataArray(fromArray, *toArray, _encodeAsLibraryTypes);
		else if (!fromArray.isByteArray() && (
				fromArray.location() == DataLocation::Memory ||
				fromArray.baseType()->storageBytes() > 16
		))
			return abiEncodingFunctionSimpleArray(fromArray, *toArray, _encodeAsLibraryTypes);
		else if (fromArray.location() == DataLocation::Memory)
			return abiEncodingFunctionMemoryByteArray(fromArray, *toArray, _encodeAsLibraryTypes);
		else if (fromArray.location() == DataLocation::Storage)
			return abiEncodingFunctionCompactStorageArray(fromArray, *toArray, _encodeAsLibraryTypes);
		else
			solAssert(false, "");
	}
	else if (auto const* toStruct = dynamic_cast<StructType const*>(&to))
	{
		StructType const* fromStruct = dynamic_cast<StructType const*>(&_from);
		solAssert(fromStruct, "");
		return abiEncodingFunctionStruct(*fromStruct, *toStruct, _encodeAsLibraryTypes);
	}
	else if (_from.category() == Type::Category::Function)
		return abiEncodingFunctionFunctionType(
			dynamic_cast<FunctionType const&>(_from),
			to,
			_encodeAsLibraryTypes,
			_fromStack
		);

	solAssert(_from.sizeOnStack() == 1, "");
	solAssert(to.isValueType(), "");
	solAssert(to.calldataEncodedSize() == 32, "");
	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");
	return createFunction(functionName, [&]() {
		solAssert(!to.isDynamicallyEncoded(), "");

		Whiskers templ(R"(
			function <functionName>(value, pos) {
				mstore(pos, <cleanupConvert>)
			}
		)");
		templ("functionName", functionName);

		if (_from.dataStoredIn(DataLocation::Storage) && to.isValueType())
		{
			// special case: convert storage reference type to value type - this is only
			// possible for library calls where we just forward the storage reference
			solAssert(_encodeAsLibraryTypes, "");
			solAssert(to == IntegerType(256), "");
			templ("cleanupConvert", "value");
		}
		else
		{
			if (_from == to)
				templ("cleanupConvert", cleanupFunction(_from) + "(value)");
			else
				templ("cleanupConvert", conversionFunction(_from, to) + "(value)");
		}
		return templ.render();
	});
}

string ABIFunctions::abiEncodingFunctionCalldataArray(
	Type const& _from,
	Type const& _to,
	bool _encodeAsLibraryTypes
)
{
	solAssert(_to.isDynamicallySized(), "");
	solAssert(_from.category() == Type::Category::Array, "Unknown dynamic type.");
	solAssert(_to.category() == Type::Category::Array, "Unknown dynamic type.");
	auto const& fromArrayType = dynamic_cast<ArrayType const&>(_from);
	auto const& toArrayType = dynamic_cast<ArrayType const&>(_to);

	solAssert(fromArrayType.location() == DataLocation::CallData, "");

	solAssert(
		*fromArrayType.copyForLocation(DataLocation::Memory, true) ==
		*toArrayType.copyForLocation(DataLocation::Memory, true),
		""
	);

	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");
	return createFunction(functionName, [&]() {
		solUnimplementedAssert(fromArrayType.isByteArray(), "Only byte arrays can be encoded from calldata currently.");
		// TODO if this is not a byte array, we might just copy byte-by-byte anyway,
		// because the encoding is position-independent, but we have to check that.
		Whiskers templ(R"(
			// <readableTypeNameFrom> -> <readableTypeNameTo>
			function <functionName>(start, length, pos) -> end {
				<storeLength> // might update pos
				<copyFun>(start, pos, length)
				end := add(pos, <roundUpFun>(length))
			}
		)");
		templ("storeLength", _to.isDynamicallySized() ? "mstore(pos, length) pos := add(pos, 0x20)" : "");
		templ("functionName", functionName);
		templ("readableTypeNameFrom", _from.toString(true));
		templ("readableTypeNameTo", _to.toString(true));
		templ("copyFun", copyToMemoryFunction(true));
		templ("roundUpFun", roundUpFunction());
		return templ.render();
	});
}

string ABIFunctions::abiEncodingFunctionSimpleArray(
	ArrayType const& _from,
	ArrayType const& _to,
	bool _encodeAsLibraryTypes
)
{
	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");

	solAssert(_from.isDynamicallySized() == _to.isDynamicallySized(), "");
	solAssert(_from.length() == _to.length(), "");
	solAssert(_from.dataStoredIn(DataLocation::Memory) || _from.dataStoredIn(DataLocation::Storage), "");
	solAssert(!_from.isByteArray(), "");
	solAssert(_from.dataStoredIn(DataLocation::Memory) || _from.baseType()->storageBytes() > 16, "");

	return createFunction(functionName, [&]() {
		bool dynamic = _to.isDynamicallyEncoded();
		bool dynamicBase = _to.baseType()->isDynamicallyEncoded();
		bool inMemory = _from.dataStoredIn(DataLocation::Memory);
		Whiskers templ(
			dynamicBase ?
			R"(
				// <readableTypeNameFrom> -> <readableTypeNameTo>
				function <functionName>(value, pos) <return> {
					let length := <lengthFun>(value)
					<storeLength> // might update pos
					let headStart := pos
					let tail := add(pos, mul(length, 0x20))
					let srcPtr := <dataAreaFun>(value)
					for { let i := 0 } lt(i, length) { i := add(i, 1) }
					{
						mstore(pos, sub(tail, headStart))
						tail := <encodeToMemoryFun>(<arrayElementAccess>, tail)
						srcPtr := <nextArrayElement>(srcPtr)
						pos := add(pos, 0x20)
					}
					pos := tail
					<assignEnd>
				}
			)" :
			R"(
				// <readableTypeNameFrom> -> <readableTypeNameTo>
				function <functionName>(value, pos) <return> {
					let length := <lengthFun>(value)
					<storeLength> // might update pos
					let srcPtr := <dataAreaFun>(value)
					for { let i := 0 } lt(i, length) { i := add(i, 1) }
					{
						<encodeToMemoryFun>(<arrayElementAccess>, pos)
						srcPtr := <nextArrayElement>(srcPtr)
						pos := add(pos, <elementEncodedSize>)
					}
					<assignEnd>
				}
			)"
		);
		templ("functionName", functionName);
		templ("readableTypeNameFrom", _from.toString(true));
		templ("readableTypeNameTo", _to.toString(true));
		templ("return", dynamic ? " -> end " : "");
		templ("assignEnd", dynamic ? "end := pos" : "");
		templ("lengthFun", arrayLengthFunction(_from));
		if (_to.isDynamicallySized())
			templ("storeLength", "mstore(pos, length) pos := add(pos, 0x20)");
		else
			templ("storeLength", "");
		templ("dataAreaFun", arrayDataAreaFunction(_from));
		templ("elementEncodedSize", toCompactHexWithPrefix(_to.baseType()->calldataEncodedSize()));
		templ("encodeToMemoryFun", abiEncodingFunction(
			*_from.baseType(),
			*_to.baseType(),
			_encodeAsLibraryTypes,
			false
		));
		templ("arrayElementAccess", inMemory ? "mload(srcPtr)" : _from.baseType()->isValueType() ? "sload(srcPtr)" : "srcPtr" );
		templ("nextArrayElement", nextArrayElementFunction(_from));
		return templ.render();
	});
}

string ABIFunctions::abiEncodingFunctionMemoryByteArray(
	ArrayType const& _from,
	ArrayType const& _to,
	bool _encodeAsLibraryTypes
)
{
	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");

	solAssert(_from.isDynamicallySized() == _to.isDynamicallySized(), "");
	solAssert(_from.length() == _to.length(), "");
	solAssert(_from.dataStoredIn(DataLocation::Memory), "");
	solAssert(_from.isByteArray(), "");

	return createFunction(functionName, [&]() {
		solAssert(_to.isByteArray(), "");
		Whiskers templ(R"(
			function <functionName>(value, pos) -> end {
				let length := <lengthFun>(value)
				mstore(pos, length)
				<copyFun>(add(value, 0x20), add(pos, 0x20), length)
				end := add(add(pos, 0x20), <roundUpFun>(length))
			}
		)");
		templ("functionName", functionName);
		templ("lengthFun", arrayLengthFunction(_from));
		templ("copyFun", copyToMemoryFunction(false));
		templ("roundUpFun", roundUpFunction());
		return templ.render();
	});
}

string ABIFunctions::abiEncodingFunctionCompactStorageArray(
	ArrayType const& _from,
	ArrayType const& _to,
	bool _encodeAsLibraryTypes
)
{
	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");

	solAssert(_from.isDynamicallySized() == _to.isDynamicallySized(), "");
	solAssert(_from.length() == _to.length(), "");
	solAssert(_from.dataStoredIn(DataLocation::Storage), "");

	return createFunction(functionName, [&]() {
		if (_from.isByteArray())
		{
			solAssert(_to.isByteArray(), "");
			Whiskers templ(R"(
				// <readableTypeNameFrom> -> <readableTypeNameTo>
				function <functionName>(value, pos) -> ret {
					let slotValue := sload(value)
					switch and(slotValue, 1)
					case 0 {
						// short byte array
						let length := and(div(slotValue, 2), 0x7f)
						mstore(pos, length)
						mstore(add(pos, 0x20), and(slotValue, not(0xff)))
						ret := add(pos, 0x40)
					}
					case 1 {
						// long byte array
						let length := div(slotValue, 2)
						mstore(pos, length)
						pos := add(pos, 0x20)
						let dataPos := <arrayDataSlot>(value)
						let i := 0
						for { } lt(i, length) { i := add(i, 0x20) } {
							mstore(add(pos, i), sload(dataPos))
							dataPos := add(dataPos, 1)
						}
						ret := add(pos, i)
					}
				}
			)");
			templ("functionName", functionName);
			templ("readableTypeNameFrom", _from.toString(true));
			templ("readableTypeNameTo", _to.toString(true));
			templ("arrayDataSlot", arrayDataAreaFunction(_from));
			return templ.render();
		}
		else
		{
			// Multiple items per slot
			solAssert(_from.baseType()->storageBytes() <= 16, "");
			solAssert(!_from.baseType()->isDynamicallyEncoded(), "");
			solAssert(_from.baseType()->isValueType(), "");
			bool dynamic = _to.isDynamicallyEncoded();
			size_t storageBytes = _from.baseType()->storageBytes();
			size_t itemsPerSlot = 32 / storageBytes;
			// This always writes full slot contents to memory, which might be
			// more than desired, i.e. it writes beyond the end of memory.
			Whiskers templ(
				R"(
					// <readableTypeNameFrom> -> <readableTypeNameTo>
					function <functionName>(value, pos) <return> {
						let length := <lengthFun>(value)
						<storeLength> // might update pos
						let originalPos := pos
						let srcPtr := <dataArea>(value)
						for { let i := 0 } lt(i, length) { i := add(i, <itemsPerSlot>) }
						{
							let data := sload(srcPtr)
							<#items>
								<encodeToMemoryFun>(<shiftRightFun>(data), pos)
								pos := add(pos, <elementEncodedSize>)
							</items>
							srcPtr := add(srcPtr, 1)
						}
						pos := add(originalPos, mul(length, <elementEncodedSize>))
						<assignEnd>
					}
				)"
			);
			templ("functionName", functionName);
			templ("readableTypeNameFrom", _from.toString(true));
			templ("readableTypeNameTo", _to.toString(true));
			templ("return", dynamic ? " -> end " : "");
			templ("assignEnd", dynamic ? "end := pos" : "");
			templ("lengthFun", arrayLengthFunction(_from));
			if (_to.isDynamicallySized())
				templ("storeLength", "mstore(pos, length) pos := add(pos, 0x20)");
			else
				templ("storeLength", "");
			templ("dataArea", arrayDataAreaFunction(_from));
			templ("itemsPerSlot", to_string(itemsPerSlot));
			string elementEncodedSize = toCompactHexWithPrefix(_to.baseType()->calldataEncodedSize());
			templ("elementEncodedSize", elementEncodedSize);
			string encodeToMemoryFun = abiEncodingFunction(
				*_from.baseType(),
				*_to.baseType(),
				_encodeAsLibraryTypes,
				false
			);
			templ("encodeToMemoryFun", encodeToMemoryFun);
			std::vector<std::map<std::string, std::string>> items(itemsPerSlot);
			for (size_t i = 0; i < itemsPerSlot; ++i)
				items[i]["shiftRightFun"] = shiftRightFunction(i * storageBytes * 8, false);
			templ("items", items);
			return templ.render();
		}
	});
}

string ABIFunctions::abiEncodingFunctionStruct(
	StructType const& _from,
	StructType const& _to,
	bool _encodeAsLibraryTypes
)
{
	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");

	solUnimplementedAssert(!_from.dataStoredIn(DataLocation::CallData), "Encoding struct from calldata is not yet supported.");
	solAssert(&_from.structDefinition() == &_to.structDefinition(), "");

	return createFunction(functionName, [&]() {
		bool fromStorage = _from.location() == DataLocation::Storage;
		bool dynamic = _to.isDynamicallyEncoded();
		Whiskers templ(R"(
			// <readableTypeNameFrom> -> <readableTypeNameTo>
			function <functionName>(value, pos) <return> {
				let tail := add(pos, <headSize>)
				<init>
				<#members>
				{
					// <memberName>
					<encode>
				}
				</members>
				<assignEnd>
			}
		)");
		templ("functionName", functionName);
		templ("readableTypeNameFrom", _from.toString(true));
		templ("readableTypeNameTo", _to.toString(true));
		templ("return", dynamic ? " -> end " : "");
		templ("assignEnd", dynamic ? "end := tail" : "");
		// to avoid multiple loads from the same slot for subsequent members
		templ("init", fromStorage ? "let slotValue := 0" : "");
		u256 previousSlotOffset(-1);
		u256 encodingOffset = 0;
		vector<map<string, string>> members;
/*
		for (auto const& member: _to.members(nullptr))
		{
			solAssert(member.type, "");
			if (!member.type->canLiveOutsideStorage())
				continue;
			solUnimplementedAssert(
				member.type->mobileType() &&
				member.type->mobileType()->interfaceType(_encodeAsLibraryTypes) &&
				member.type->mobileType()->interfaceType(_encodeAsLibraryTypes)->encodingType(),
				"Encoding type \"" + member.type->toString() + "\" not yet implemented."
			);
			auto memberTypeTo = member.type->mobileType()->interfaceType(_encodeAsLibraryTypes)->encodingType();
			auto memberTypeFrom = _from.memberType(member.name);
			solAssert(memberTypeFrom, "");
			bool dynamicMember = memberTypeTo->isDynamicallyEncoded();
			if (dynamicMember)
				solAssert(dynamic, "");
			Whiskers memberTempl(R"(
				<preprocess>
				let memberValue := <retrieveValue>
				)" + (
					dynamicMember ?
					string(R"(
						mstore(add(pos, <encodingOffset>), sub(tail, pos))
						tail := <abiEncode>(memberValue, tail)
					)") :
					string(R"(
						<abiEncode>(memberValue, add(pos, <encodingOffset>))
					)")
				)
			);
			if (fromStorage)
			{
				solAssert(memberTypeFrom->isValueType() == memberTypeTo->isValueType(), "");
				u256 storageSlotOffset;
				size_t intraSlotOffset;
				tie(storageSlotOffset, intraSlotOffset) = _from.storageOffsetsOfMember(member.name);
				if (memberTypeFrom->isValueType())
				{
					if (storageSlotOffset != previousSlotOffset)
					{
						memberTempl("preprocess", "slotValue := sload(add(value, " + toCompactHexWithPrefix(storageSlotOffset) + "))");
						previousSlotOffset = storageSlotOffset;
					}
					else
						memberTempl("preprocess", "");
					memberTempl("retrieveValue", shiftRightFunction(intraSlotOffset * 8, false) + "(slotValue)");
				}
				else
				{
					solAssert(memberTypeFrom->dataStoredIn(DataLocation::Storage), "");
					solAssert(intraSlotOffset == 0, "");
					memberTempl("preprocess", "");
					memberTempl("retrieveValue", "add(value, " + toCompactHexWithPrefix(storageSlotOffset) + ")");
				}
			}
			else
			{
				memberTempl("preprocess", "");
				string sourceOffset = toCompactHexWithPrefix(_from.memoryOffsetOfMember(member.name));
				memberTempl("retrieveValue", "mload(add(value, " + sourceOffset + "))");
			}
			memberTempl("encodingOffset", toCompactHexWithPrefix(encodingOffset));
			encodingOffset += dynamicMember ? 0x20 : memberTypeTo->calldataEncodedSize();
			memberTempl("abiEncode", abiEncodingFunction(*memberTypeFrom, *memberTypeTo, _encodeAsLibraryTypes, false));

			members.push_back({});
			members.back()["encode"] = memberTempl.render();
			members.back()["memberName"] = member.name;
		}
*/
		templ("members", members);
		templ("headSize", toCompactHexWithPrefix(encodingOffset));
		return templ.render();
	});
}

string ABIFunctions::abiEncodingFunctionStringLiteral(
	Type const& _from,
	Type const& _to,
	bool _encodeAsLibraryTypes
)
{
	solAssert(_from.category() == Type::Category::StringLiteral, "");

	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_encodeAsLibraryTypes ? "_library" : "");
	return createFunction(functionName, [&]() {
		auto const& strType = dynamic_cast<StringLiteralType const&>(_from);
		string const& value = strType.value();
		solAssert(_from.sizeOnStack() == 0, "");

		if (_to.isDynamicallySized())
		{
			Whiskers templ(R"(
				function <functionName>(pos) -> end {
					mstore(pos, <length>)
					<#word>
						mstore(add(pos, <offset>), <wordValue>)
					</word>
					end := add(pos, <overallSize>)
				}
			)");
			templ("functionName", functionName);

			// TODO this can make use of CODECOPY for large strings once we have that in JULIA
			size_t words = (value.size() + 31) / 32;
			templ("overallSize", to_string(32 + words * 32));
			templ("length", to_string(value.size()));
			vector<map<string, string>> wordParams(words);
			for (size_t i = 0; i < words; ++i)
			{
				wordParams[i]["offset"] = to_string(32 + i * 32);
				wordParams[i]["wordValue"] = "0x" + h256(value.substr(32 * i, 32), h256::AlignLeft).hex();
			}
			templ("word", wordParams);
			return templ.render();
		}
		else
		{
			solAssert(_to.category() == Type::Category::FixedBytes, "");
			solAssert(value.size() <= 32, "");
			Whiskers templ(R"(
				function <functionName>(pos) {
					mstore(pos, <wordValue>)
				}
			)");
			templ("functionName", functionName);
			templ("wordValue", "0x" + h256(value, h256::AlignLeft).hex());
			return templ.render();
		}
	});
}

string ABIFunctions::abiEncodingFunctionFunctionType(
	FunctionType const& _from,
	Type const& _to,
	bool _encodeAsLibraryTypes,
	bool _fromStack
)
{
	solAssert(_from.kind() == FunctionType::Kind::External, "");
	solAssert(_from == _to, "");

	string functionName =
		"abi_encode_" +
		_from.identifier() +
		"_to_" +
		_to.identifier() +
		(_fromStack ? "_fromStack" : "") +
		(_encodeAsLibraryTypes ? "_library" : "");

	if (_fromStack)
		return createFunction(functionName, [&]() {
			return Whiskers(R"(
				function <functionName>(addr, function_id, pos) {
					mstore(pos, <combineExtFun>(addr, function_id))
				}
			)")
			("functionName", functionName)
			("combineExtFun", combineExternalFunctionIdFunction())
			.render();
		});
	else
		return createFunction(functionName, [&]() {
			return Whiskers(R"(
				function <functionName>(addr_and_function_id, pos) {
					mstore(pos, <cleanExtFun>(addr_and_function_id))
				}
			)")
			("functionName", functionName)
			("cleanExtFun", cleanupCombinedExternalFunctionIdFunction())
			.render();
		});
}

string ABIFunctions::abiDecodingFunction(Type const& _type, bool _fromMemory, bool _forUseOnStack)
{
	// The decoding function has to perform bounds checks unless it decodes a value type.
	// Conversely, bounds checks have to be performed before the decoding function
	// of a value type is called.

	TypePointer decodingType = _type.decodingType();
	solAssert(decodingType, "");

	if (auto arrayType = dynamic_cast<ArrayType const*>(decodingType.get()))
	{
		if (arrayType->dataStoredIn(DataLocation::CallData))
		{
			solAssert(!_fromMemory, "");
			return abiDecodingFunctionCalldataArray(*arrayType);
		}
		else if (arrayType->isByteArray())
			return abiDecodingFunctionByteArray(*arrayType, _fromMemory);
		else
			return abiDecodingFunctionArray(*arrayType, _fromMemory);
	}
	else if (auto const* structType = dynamic_cast<StructType const*>(decodingType.get()))
		return abiDecodingFunctionStruct(*structType, _fromMemory);
	else if (auto const* functionType = dynamic_cast<FunctionType const*>(decodingType.get()))
		return abiDecodingFunctionFunctionType(*functionType, _fromMemory, _forUseOnStack);
	else
		return abiDecodingFunctionValueType(_type, _fromMemory);
}

string ABIFunctions::abiDecodingFunctionValueType(const Type& _type, bool _fromMemory)
{
	TypePointer decodingType = _type.decodingType();
	solAssert(decodingType, "");
	solAssert(decodingType->sizeOnStack() == 1, "");
	solAssert(decodingType->isValueType(), "");
	solAssert(decodingType->calldataEncodedSize() == 32, "");
	solAssert(!decodingType->isDynamicallyEncoded(), "");

	string functionName =
		"abi_decode_" +
		_type.identifier() +
		(_fromMemory ? "_fromMemory" : "");
	return createFunction(functionName, [&]() {
		Whiskers templ(R"(
			function <functionName>(offset, end) -> value {
				value := <cleanup>(<load>(offset))
			}
		)");
		templ("functionName", functionName);
		templ("load", _fromMemory ? "mload" : "calldataload");
		// Cleanup itself should use the type and not decodingType, because e.g.
		// the decoding type of an enum is a plain int.
		templ("cleanup", cleanupFunction(_type, true));
		return templ.render();
	});

}

string ABIFunctions::abiDecodingFunctionArray(ArrayType const& _type, bool _fromMemory)
{
	solAssert(_type.dataStoredIn(DataLocation::Memory), "");
	solAssert(!_type.isByteArray(), "");

	string functionName =
		"abi_decode_" +
		_type.identifier() +
		(_fromMemory ? "_fromMemory" : "");

	solAssert(!_type.dataStoredIn(DataLocation::Storage), "");

	return createFunction(functionName, [&]() {
		string load = _fromMemory ? "mload" : "calldataload";
		bool dynamicBase = _type.baseType()->isDynamicallyEncoded();
		Whiskers templ(
			R"(
				// <readableTypeName>
				function <functionName>(offset, end) -> array {
					if iszero(slt(add(offset, 0x1f), end)) { revert(0, 0) }
					let length := <retrieveLength>
					array := <allocate>(<allocationSize>(length))
					let dst := array
					<storeLength> // might update offset and dst
					let src := offset
					<staticBoundsCheck>
					for { let i := 0 } lt(i, length) { i := add(i, 1) }
					{
						let elementPos := <retrieveElementPos>
						mstore(dst, <decodingFun>(elementPos, end))
						dst := add(dst, 0x20)
						src := add(src, <baseEncodedSize>)
					}
				}
			)"
		);
/*
		templ("functionName", functionName);
		templ("readableTypeName", _type.toString(true));
		templ("retrieveLength", !_type.isDynamicallySized() ? toCompactHexWithPrefix(_type.length()) : load + "(offset)");
		templ("allocate", allocationFunction());
		templ("allocationSize", arrayAllocationSizeFunction(_type));
		if (_type.isDynamicallySized())
			templ("storeLength", "mstore(array, length) offset := add(offset, 0x20) dst := add(dst, 0x20)");
		else
			templ("storeLength", "");
*/
		if (dynamicBase)
		{
			templ("staticBoundsCheck", "");
			templ("retrieveElementPos", "add(offset, " + load + "(src))");
			templ("baseEncodedSize", "0x20");
		}
		else
		{
			string baseEncodedSize = toCompactHexWithPrefix(_type.baseType()->calldataEncodedSize());
			templ("staticBoundsCheck", "if gt(add(src, mul(length, " + baseEncodedSize + ")), end) { revert(0, 0) }");
			templ("retrieveElementPos", "src");
			templ("baseEncodedSize", baseEncodedSize);
		}
		templ("decodingFun", abiDecodingFunction(*_type.baseType(), _fromMemory, false));
		return templ.render();
	});
}

string ABIFunctions::abiDecodingFunctionCalldataArray(ArrayType const& _type)
{
	// This does not work with arrays of complex types - the array access
	// is not yet implemented in Solidity.
	solAssert(_type.dataStoredIn(DataLocation::CallData), "");
	if (!_type.isDynamicallySized())
		solAssert(_type.length() < u256("0xffffffffffffffff"), "");
	solAssert(!_type.baseType()->isDynamicallyEncoded(), "");
	solAssert(_type.baseType()->calldataEncodedSize() < u256("0xffffffffffffffff"), "");

	string functionName =
		"abi_decode_" +
		_type.identifier();
	return createFunction(functionName, [&]() {
		string templ;
		if (_type.isDynamicallySized())
			templ = R"(
				// <readableTypeName>
				function <functionName>(offset, end) -> arrayPos, length {
					if iszero(slt(add(offset, 0x1f), end)) { revert(0, 0) }
					length := calldataload(offset)
					if gt(length, 0xffffffffffffffff) { revert(0, 0) }
					arrayPos := add(offset, 0x20)
					if gt(add(arrayPos, mul(<length>, <baseEncodedSize>)), end) { revert(0, 0) }
				}
			)";
		else
			templ = R"(
				// <readableTypeName>
				function <functionName>(offset, end) -> arrayPos {
					arrayPos := offset
					if gt(add(arrayPos, mul(<length>, <baseEncodedSize>)), end) { revert(0, 0) }
				}
			)";
		Whiskers w{templ};
		w("functionName", functionName);
		w("readableTypeName", _type.toString(true));
		w("baseEncodedSize", toCompactHexWithPrefix(_type.isByteArray() ? 1 : _type.baseType()->calldataEncodedSize()));
/*
		w("length", _type.isDynamicallyEncoded() ? "length" : toCompactHexWithPrefix(_type.length()));
*/
		return w.render();
	});
}

string ABIFunctions::abiDecodingFunctionByteArray(ArrayType const& _type, bool _fromMemory)
{
	solAssert(_type.dataStoredIn(DataLocation::Memory), "");
	solAssert(_type.isByteArray(), "");

	string functionName =
		"abi_decode_" +
		_type.identifier() +
		(_fromMemory ? "_fromMemory" : "");

	return createFunction(functionName, [&]() {
		Whiskers templ(
			R"(
				function <functionName>(offset, end) -> array {
					if iszero(slt(add(offset, 0x1f), end)) { revert(0, 0) }
					let length := <load>(offset)
					array := <allocate>(<allocationSize>(length))
					mstore(array, length)
					let src := add(offset, 0x20)
					let dst := add(array, 0x20)
					if gt(add(src, length), end) { revert(0, 0) }
					<copyToMemFun>(src, dst, length)
				}
			)"
		);
		templ("functionName", functionName);
		templ("load", _fromMemory ? "mload" : "calldataload");
		templ("allocate", allocationFunction());
		templ("allocationSize", arrayAllocationSizeFunction(_type));
		templ("copyToMemFun", copyToMemoryFunction(!_fromMemory));
		return templ.render();
	});
}

string ABIFunctions::abiDecodingFunctionStruct(StructType const& _type, bool _fromMemory)
{
	string functionName =
		"abi_decode_" +
		_type.identifier() +
		(_fromMemory ? "_fromMemory" : "");

	solUnimplementedAssert(!_type.dataStoredIn(DataLocation::CallData), "");

	return createFunction(functionName, [&]() {
		Whiskers templ(R"(
			// <readableTypeName>
			function <functionName>(headStart, end) -> value {
				if slt(sub(end, headStart), <minimumSize>) { revert(0, 0) }
				value := <allocate>(<memorySize>)
				<#members>
				{
					// <memberName>
					<decode>
				}
				</members>
			}
		)");
		templ("functionName", functionName);
		templ("readableTypeName", _type.toString(true));
		templ("allocate", allocationFunction());
/*
		solAssert(_type.memorySize() < u256("0xffffffffffffffff"), "");
		templ("memorySize", toCompactHexWithPrefix(_type.memorySize()));
*/
		size_t headPos = 0;
		vector<map<string, string>> members;
		for (auto const& member: _type.members(nullptr))
		{
			solAssert(member.type, "");
			solAssert(member.type->canLiveOutsideStorage(), "");
			auto decodingType = member.type->decodingType();
			solAssert(decodingType, "");
			bool dynamic = decodingType->isDynamicallyEncoded();
			Whiskers memberTempl(
				dynamic ?
				R"(
					let offset := <load>(add(headStart, <pos>))
					if gt(offset, 0xffffffffffffffff) { revert(0, 0) }
					mstore(add(value, <memoryOffset>), <abiDecode>(add(headStart, offset), end))
				)" :
				R"(
					let offset := <pos>
					mstore(add(value, <memoryOffset>), <abiDecode>(add(headStart, offset), end))
				)"
			);
			memberTempl("load", _fromMemory ? "mload" : "calldataload");
			memberTempl("pos", to_string(headPos));
/*
			memberTempl("memoryOffset", toCompactHexWithPrefix(_type.memoryOffsetOfMember(member.name)));
*/
			memberTempl("abiDecode", abiDecodingFunction(*member.type, _fromMemory, false));

			members.push_back({});
			members.back()["decode"] = memberTempl.render();
			members.back()["memberName"] = member.name;
			headPos += dynamic ? 0x20 : decodingType->calldataEncodedSize();
		}
		templ("members", members);
		templ("minimumSize", toCompactHexWithPrefix(headPos));
		return templ.render();
	});
}

string ABIFunctions::abiDecodingFunctionFunctionType(FunctionType const& _type, bool _fromMemory, bool _forUseOnStack)
{
	solAssert(_type.kind() == FunctionType::Kind::External, "");

	string functionName =
		"abi_decode_" +
		_type.identifier() +
		(_fromMemory ? "_fromMemory" : "") +
		(_forUseOnStack ? "_onStack" : "");

	return createFunction(functionName, [&]() {
		if (_forUseOnStack)
		{
			return Whiskers(R"(
				function <functionName>(offset, end) -> addr, function_selector {
					addr, function_selector := <splitExtFun>(<load>(offset))
				}
			)")
			("functionName", functionName)
			("load", _fromMemory ? "mload" : "calldataload")
			("splitExtFun", splitExternalFunctionIdFunction())
			.render();
		}
		else
		{
			return Whiskers(R"(
				function <functionName>(offset, end) -> fun {
					fun := <cleanExtFun>(<load>(offset))
				}
			)")
			("functionName", functionName)
			("load", _fromMemory ? "mload" : "calldataload")
			("cleanExtFun", cleanupCombinedExternalFunctionIdFunction())
			.render();
		}
	});
}

string ABIFunctions::copyToMemoryFunction(bool _fromCalldata)
{
	string functionName = "copy_" + string(_fromCalldata ? "calldata" : "memory") + "_to_memory";
	return createFunction(functionName, [&]() {
		if (_fromCalldata)
		{
			return Whiskers(R"(
				function <functionName>(src, dst, length) {
					calldatacopy(dst, src, length)
					// clear end
					mstore(add(dst, length), 0)
				}
			)")
			("functionName", functionName)
			.render();
		}
		else
		{
			return Whiskers(R"(
				function <functionName>(src, dst, length) {
					let i := 0
					for { } lt(i, length) { i := add(i, 32) }
					{
						mstore(add(dst, i), mload(add(src, i)))
					}
					if gt(i, length)
					{
						// clear end
						mstore(add(dst, length), 0)
					}
				}
			)")
			("functionName", functionName)
			.render();
		}
	});
}

string ABIFunctions::shiftLeftFunction(size_t _numBits)
{
	string functionName = "shift_left_" + to_string(_numBits);
	return createFunction(functionName, [&]() {
		solAssert(_numBits < 256, "");
		return
			Whiskers(R"(
			function <functionName>(value) -> newValue {
				newValue := mul(value, <multiplier>)
			}
			)")
			("functionName", functionName)
			("multiplier", toCompactHexWithPrefix(u256(1) << _numBits))
			.render();
	});
}

string ABIFunctions::shiftRightFunction(size_t _numBits, bool _signed)
{
	string functionName = "shift_right_" + to_string(_numBits) + (_signed ? "_signed" : "_unsigned");
	return createFunction(functionName, [&]() {
		solAssert(_numBits < 256, "");
		return
			Whiskers(R"(
			function <functionName>(value) -> newValue {
				newValue := <div>(value, <multiplier>)
			}
			)")
			("functionName", functionName)
			("div", _signed ? "sdiv" : "div")
			("multiplier", toCompactHexWithPrefix(u256(1) << _numBits))
			.render();
	});
}

string ABIFunctions::roundUpFunction()
{
	string functionName = "round_up_to_mul_of_32";
	return createFunction(functionName, [&]() {
		return
			Whiskers(R"(
			function <functionName>(value) -> result {
				result := and(add(value, 31), not(31))
			}
			)")
			("functionName", functionName)
			.render();
	});
}

string ABIFunctions::arrayLengthFunction(ArrayType const& _type)
{
	string functionName = "array_length_" + _type.identifier();
	return createFunction(functionName, [&]() {
		Whiskers w(R"(
			function <functionName>(value) -> length {
				<body>
			}
		)");
		w("functionName", functionName);
		string body;
/*
		if (!_type.isDynamicallySized())
			body = "length := " + toCompactHexWithPrefix(_type.length());
		else
		{
			switch (_type.location())
			{
			case DataLocation::CallData:
				solAssert(false, "called regular array length function on calldata array");
				break;
			case DataLocation::Memory:
				body = "length := mload(value)";
				break;
			case DataLocation::Storage:
				if (_type.isByteArray())
				{
					// Retrieve length both for in-place strings and off-place strings:
					// Computes (x & (0x100 * (ISZERO (x & 1)) - 1)) / 2
					// i.e. for short strings (x & 1 == 0) it does (x & 0xff) / 2 and for long strings it
					// computes (x & (-1)) / 2, which is equivalent to just x / 2.
					body = R"(
						length := sload(value)
						let mask := sub(mul(0x100, iszero(and(length, 1))), 1)
						length := div(and(length, mask), 2)
					)";
				}
				else
					body = "length := sload(value)";
				break;
			}
		}
*/
		solAssert(!body.empty(), "");
		w("body", body);
		return w.render();
	});
}

string ABIFunctions::arrayAllocationSizeFunction(ArrayType const& _type)
{
	solAssert(_type.dataStoredIn(DataLocation::Memory), "");
	string functionName = "array_allocation_size_" + _type.identifier();
	return createFunction(functionName, [&]() {
		Whiskers w(R"(
			function <functionName>(length) -> size {
				// Make sure we can allocate memory without overflow
				if gt(length, 0xffffffffffffffff) { revert(0, 0) }
				size := <allocationSize>
				<addLengthSlot>
			}
		)");
		w("functionName", functionName);
		if (_type.isByteArray())
			// Round up
			w("allocationSize", "and(add(length, 0x1f), not(0x1f))");
		else
			w("allocationSize", "mul(length, 0x20)");
		if (_type.isDynamicallySized())
			w("addLengthSlot", "size := add(size, 0x20)");
		else
			w("addLengthSlot", "");
		return w.render();
	});
}

string ABIFunctions::arrayDataAreaFunction(ArrayType const& _type)
{
	string functionName = "array_dataslot_" + _type.identifier();
	return createFunction(functionName, [&]() {
		if (_type.dataStoredIn(DataLocation::Memory))
		{
			if (_type.isDynamicallySized())
				return Whiskers(R"(
					function <functionName>(memPtr) -> dataPtr {
						dataPtr := add(memPtr, 0x20)
					}
				)")
				("functionName", functionName)
				.render();
			else
				return Whiskers(R"(
					function <functionName>(memPtr) -> dataPtr {
						dataPtr := memPtr
					}
				)")
				("functionName", functionName)
				.render();
		}
		else if (_type.dataStoredIn(DataLocation::Storage))
		{
			if (_type.isDynamicallySized())
			{
				Whiskers w(R"(
					function <functionName>(slot) -> dataSlot {
						mstore(0, slot)
						dataSlot := keccak256(0, 0x20)
					}
				)");
				w("functionName", functionName);
				return w.render();
			}
			else
			{
				Whiskers w(R"(
					function <functionName>(slot) -> dataSlot {
						dataSlot := slot
					}
				)");
				w("functionName", functionName);
				return w.render();
			}
		}
		else
		{
			// Not used for calldata
			solAssert(false, "");
		}
	});
}

string ABIFunctions::nextArrayElementFunction(ArrayType const& _type)
{
	solAssert(!_type.isByteArray(), "");
	solAssert(
		_type.location() == DataLocation::Memory ||
		_type.location() == DataLocation::Storage,
		""
	);
	solAssert(
		_type.location() == DataLocation::Memory ||
		_type.baseType()->storageBytes() > 16,
		""
	);
	string functionName = "array_nextElement_" + _type.identifier();
	return createFunction(functionName, [&]() {
		if (_type.location() == DataLocation::Memory)
			return Whiskers(R"(
				function <functionName>(memPtr) -> nextPtr {
					nextPtr := add(memPtr, 0x20)
				}
			)")
			("functionName", functionName)
			.render();
		else if (_type.location() == DataLocation::Storage)
			return Whiskers(R"(
				function <functionName>(slot) -> nextSlot {
					nextSlot := add(slot, 1)
				}
			)")
			("functionName", functionName)
			.render();
		else
			solAssert(false, "");
	});
}

string ABIFunctions::allocationFunction()
{
	string functionName = "allocateMemory";
	return createFunction(functionName, [&]() {
		return Whiskers(R"(
			function <functionName>(size) -> memPtr {
				memPtr := mload(<freeMemoryPointer>)
				let newFreePtr := add(memPtr, size)
				// protect against overflow
				if or(gt(newFreePtr, 0xffffffffffffffff), lt(newFreePtr, memPtr)) { revert(0, 0) }
				mstore(<freeMemoryPointer>, newFreePtr)
			}
		)")
		("freeMemoryPointer", to_string(CompilerUtils::freeMemoryPointer))
		("functionName", functionName)
		.render();
	});
}

string ABIFunctions::createFunction(string const& _name, function<string ()> const& _creator)
{
	if (!m_requestedFunctions.count(_name))
	{
		auto fun = _creator();
		solAssert(!fun.empty(), "");
		m_requestedFunctions[_name] = fun;
	}
	return _name;
}

size_t ABIFunctions::headSize(TypePointers const& _targetTypes)
{
	size_t headSize = 0;
	for (auto const& t: _targetTypes)
	{
		if (t->isDynamicallyEncoded())
			headSize += 0x20;
		else
			headSize += t->calldataEncodedSize();
	}

	return headSize;
}
