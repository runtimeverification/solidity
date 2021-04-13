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

#include <test/libsolidity/util/BytesUtils.h>

#include <test/libsolidity/util/ContractABIUtils.h>
#include <test/libsolidity/util/SoltestErrors.h>
#include <test/ExecutionFramework.h>

#include <liblangutil/Common.h>

#include <libsolutil/CommonData.h>
#include <libsolutil/StringUtils.h>

#include <boost/algorithm/string.hpp>

#include <fstream>
#include <iomanip>
#include <memory>
#include <regex>
#include <stdexcept>

using namespace solidity;
using namespace solidity::util;
using namespace solidity::frontend;
using namespace solidity::frontend::test;
using namespace soltest;
using namespace std;

bytes BytesUtils::alignLeft(bytes _bytes)
{
	soltestAssert(_bytes.size() <= 32, "");
	size_t size = _bytes.size();
	return std::move(_bytes) + bytes(32 - size, 0);
}

bytes BytesUtils::alignRight(bytes _bytes)
{
	soltestAssert(_bytes.size() <= 32, "");
	return bytes(32 - _bytes.size(), 0) + std::move(_bytes);
}

bytes BytesUtils::applyAlign(
	Parameter::Alignment _alignment,
	ABIType& _abiType,
	bytes _bytes
)
{
	if (_alignment != Parameter::Alignment::None)
		_abiType.alignDeclared = true;

	switch (_alignment)
	{
	case Parameter::Alignment::Left:
		_abiType.align = ABIType::AlignLeft;
		return alignLeft(std::move(_bytes));
	case Parameter::Alignment::Right:
	default:
		_abiType.align = ABIType::AlignRight;
		return alignRight(std::move(_bytes));
	}
}

bytes BytesUtils::convertBoolean(string const& _literal)
{
	if (_literal == "true")
		return solidity::test::ExecutionFramework::encode(true);
	else if (_literal == "false")
		return solidity::test::ExecutionFramework::encode(false);
	else
		throw TestParserError("Boolean literal invalid.");
}

bytes BytesUtils::convertNumber(string const& _literal)
{
	try
	{
		return solidity::test::ExecutionFramework::encode(bigint(_literal));
	}
	catch (std::exception const&)
	{
		throw TestParserError("Number encoding invalid.");
	}
}

bytes BytesUtils::convertHexNumber(string const& _literal)
{
	try
	{
		return fromHex(_literal);
	}
	catch (std::exception const&)
	{
		throw TestParserError("Hex number encoding invalid.");
	}
}

bytes BytesUtils::convertString(string const& _literal)
{
	try
	{
		return solidity::test::ExecutionFramework::encodeDyn(_literal);
	}
	catch (std::exception const&)
	{
		throw TestParserError("String encoding invalid.");
	}
}

bytes BytesUtils::convertErrorMessage(string const& _literal)
{
	try
	{
		return solidity::test::ExecutionFramework::encodeLogs(_literal);
	}
	catch (std::exception const&)
	{
		throw TestParserError("Error message encoding invalid.");
	}
}

bytes BytesUtils::convertArray(vector<pair<string, Token>> const& _literal, bool isDynamic)
{
	size_t elemSize = (size_t) u256(_literal[0].first) / 8;
	vector<u256> elems;
	for (auto it = _literal.begin()+1; it != _literal.end(); ++it)
	{
		switch (it->second)
		{
		case Token::Boolean:
			elems.push_back(it->first == "true" ? u256(1) : u256(0));
			break;
		case Token::HexNumber:
		case Token::Number:
			elems.push_back(u256(it->first));
			break;
		default:
			throw TestParserError("Array encoding invalid.");
		}
	}
	if (isDynamic)
		return solidity::test::ExecutionFramework::encodeRefArray(elems, elems.size(), elemSize);
	else
		return solidity::test::ExecutionFramework::encodeRefArray(elems, elemSize);
}

bytes BytesUtils::convertRefArgs(vector<pair<string, Token>> const& _literal)
{
	bytes result;
	for (auto &p : _literal)
	{
		switch (p.second)
		{
		case Token::Boolean:
		{
			bytes encoded =
				solidity::test::ExecutionFramework::encodeLog(p.first == "true" ? true : false);
			result = encoded + result;
			break;
		}
		case Token::Hex:
		case Token::HexNumber:
		{
			bytes encoded = fromHex(p.first);
			result = encoded + result;
			break;
		}
		case Token::Number:
		{
			bytes encoded = solidity::test::ExecutionFramework::encodeLog(bigint(p.first));
			result = encoded + result;
			break;
		}
		default:
			throw TestParserError("RefArgs encoding invalid.");
		}
	}
	return result;
}

string BytesUtils::formatUnsigned(bytes const& _bytes)
{
	stringstream os;

	soltestAssert(!_bytes.empty() && _bytes.size() <= 32, "");

	return fromBigEndian<u256>(_bytes).str();
}

string BytesUtils::formatSigned(bytes const& _bytes)
{
	stringstream os;

	soltestAssert(!_bytes.empty() && _bytes.size() <= 32, "");

	if (*_bytes.begin() & 0x80)
		os << u2s(fromBigEndian<u256>(_bytes));
	else
		os << fromBigEndian<u256>(_bytes);

	return os.str();
}

string BytesUtils::formatBoolean(bytes const& _bytes)
{
	stringstream os;
	u256 result = fromBigEndian<u256>(_bytes);

	if (result == 0)
		os << "false";
	else if (result == 1)
		os << "true";
	else
		os << result;

	return os.str();
}

string BytesUtils::formatHex(bytes const& _bytes, bool _shorten)
{
	soltestAssert(!_bytes.empty() && _bytes.size() <= 32, "");
	u256 value = fromBigEndian<u256>(_bytes);
	string output = toCompactHexWithPrefix(value);

	if (_shorten)
		return output.substr(0, output.size() - countRightPaddedZeros(_bytes) * 2);
	return output;
}

string BytesUtils::formatHexString(bytes const& _bytes)
{
	stringstream os;

	os << "hex\"" << toHex(_bytes) << "\"";

	return os.str();
}

string BytesUtils::formatString(bytes const& _bytes, size_t _cutOff)
{
	stringstream os;

	os << "\"";
	for (size_t i = 0; i < min(_cutOff, _bytes.size()); ++i)
	{
		auto const v = _bytes[i];
		switch (v)
		{
			case '\0':
				os << "\\0";
				break;
			case '\n':
				os << "\\n";
				break;
			default:
				if (isprint(v))
					os << v;
				else
					os << "\\x" << toHex(v);
		}
	}
	os << "\"";

	return os.str();
}

string BytesUtils::formatRawBytes(
	vector<bytes> const& _bytes,
	solidity::frontend::test::ParameterList const& _parameters,
	string _linePrefix)
{
	stringstream os;
	ParameterList parameters;
	auto it = _bytes.begin();

	bool sizesAgree = _parameters.size() == _bytes.size();
	size_t totalBytesSize = 0;
	for (unsigned i = 0; sizesAgree && i < _bytes.size(); ++i) {
		sizesAgree = sizesAgree && _bytes[i].size() == _parameters[i].abiType.size;
		totalBytesSize += _bytes[i].size();
	}

	if (!sizesAgree)
		parameters = ContractABIUtils::defaultParameters(_bytes.size());
	else {
		soltestAssert(totalBytesSize == ContractABIUtils::encodingSize(_parameters), "");
		parameters = _parameters;
	}


	for (auto const& parameter: parameters)
	{
		bytes byteRange{*it};

		os << _linePrefix << byteRange;
		if (&parameter != &parameters.back())
			os << endl;

		++it;
	}

	return os.str();
}

string BytesUtils::formatBytes(
	bytes const& _bytes,
	ABIType const& _abiType
)
{
	stringstream os;

	switch (_abiType.type)
	{
	case ABIType::UnsignedDec:
		// Check if the detected type was wrong and if this could
		// be signed. If an unsigned was detected in the expectations,
		// but the actual result returned a signed, it would be formatted
		// incorrectly.
		if (*_bytes.begin() & 0x80)
			os << formatSigned(_bytes);
		else
		{
			std::string decimal(formatUnsigned(_bytes));
			std::string hexadecimal(formatHex(_bytes));
			unsigned int value = u256(_bytes).convert_to<unsigned int>();
			if (value < 0x10)
				os << decimal;
			else if (value >= 0x10 && value <= 0xff) {
				os << hexadecimal;
			}
			else
			{
				auto entropy = [](std::string const& str) -> double {
					double result = 0;
					map<char, double> frequencies;
					for (char c: str)
						frequencies[c]++;
					for (auto p: frequencies)
					{
						double freq = p.second / double(str.length());
						result -= freq * (log(freq) / log(2.0));
					}
					return result;
				};
				if (entropy(decimal) < entropy(hexadecimal.substr(2, hexadecimal.length())))
					os << decimal;
				else
					os << hexadecimal;
			}
		}
		break;
	case ABIType::SignedDec:
		os << formatSigned(_bytes);
		break;
	case ABIType::Boolean:
		os << formatBoolean(_bytes);
		break;
	case ABIType::Hex:
		os << formatHex(_bytes, _abiType.alignDeclared);
		break;
	case ABIType::HexString:
		os << formatHexString(_bytes);
		break;
	case ABIType::String:
		os << formatString(_bytes, _bytes.size() - countRightPaddedZeros(_bytes));
		break;
	case ABIType::Failure:
		break;
	case ABIType::None:
		break;
	}

	if (_abiType.alignDeclared)
		return (_abiType.align == ABIType::AlignLeft ? "left(" : "right(") + os.str() + ")";
	return os.str();
}

string BytesUtils::formatBytesRange(
	vector<bytes> _bytes,
	solidity::frontend::test::ParameterList const& _parameters,
	bool _highlight
)
{
	stringstream os;
	ParameterList parameters;
	auto it = _bytes.begin();

	bool sizesAgree = _parameters.size() == _bytes.size();
	size_t totalBytesSize = 0;
	for (unsigned i = 0; sizesAgree && i < _bytes.size(); ++i) {
		sizesAgree = sizesAgree && _bytes[i].size() == _parameters[i].abiType.size;
		totalBytesSize += _bytes[i].size();
	}

	if (!sizesAgree)
		parameters = ContractABIUtils::defaultParameters(_bytes.size());
	else {
		soltestAssert(totalBytesSize == ContractABIUtils::encodingSize(_parameters), "");
		parameters = _parameters;
	}

	for (auto const& parameter: parameters)
	{
		bytes byteRange{*it};

		if (!parameter.matchesBytes(byteRange))
			AnsiColorized(
				os,
				_highlight,
				{util::formatting::RED_BACKGROUND}
			) << formatBytes(byteRange, parameter.abiType);
		else
			os << parameter.rawString;

		if (&parameter != &parameters.back())
			os << ", ";

		++it;
	}

	return os.str();
}

size_t BytesUtils::countRightPaddedZeros(bytes const& _bytes)
{
	return static_cast<size_t>(find_if(
		_bytes.rbegin(),
		_bytes.rend(),
		[](uint8_t b) { return b != '\0'; }
	) - _bytes.rbegin());
}

