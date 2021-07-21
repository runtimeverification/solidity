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

#include <test/libsolidity/util/TestFileParser.h>

#include <test/libsolidity/util/BytesUtils.h>
#include <test/libsolidity/util/SoltestErrors.h>
#include <test/Common.h>

#include <liblangutil/Common.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/throw_exception.hpp>

#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>

using namespace solidity;
using namespace solidity::frontend;
using namespace solidity::frontend::test;
using namespace std;

using Token = soltest::Token;

char TestFileParser::Scanner::peek() const noexcept
{
	if (std::distance(m_char, m_line.end()) < 2)
		return '\0';

	auto next = m_char;
	std::advance(next, 1);
	return *next;
}

vector<solidity::frontend::test::FunctionCall> TestFileParser::parseFunctionCalls(size_t _lineOffset)
{
	vector<FunctionCall> calls;
	if (!accept(Token::EOS))
	{
		assert(m_scanner.currentToken() == Token::Unknown);
		m_scanner.scanNextToken();

		while (!accept(Token::EOS))
		{
			if (!accept(Token::Whitespace))
			{
				FunctionCall call;

				/// If this is not the first call in the test,
				/// the last call to parseParameter could have eaten the
				/// new line already. This could only be fixed with a one
				/// token lookahead that checks parseParameter
				/// if the next token is an identifier.
				if (calls.empty())
					expect(Token::Newline);
				else
					if (accept(Token::Newline, true))
						m_lineNumber++;

				try
				{
					if (accept(Token::Library, true))
					{
						expect(Token::Colon);
						call.signature = m_scanner.currentLiteral();
						expect(Token::Identifier);
						call.kind = FunctionCall::Kind::Library;
						call.expectations.failure = false;
					}
					else if (accept(Token::Storage, true))
					{
						expect(Token::Colon);
						call.expectations.failure = false;
						call.expectations.result.push_back(Parameter());
						// empty / non-empty is encoded as false / true
						if (m_scanner.currentLiteral() == "empty")
							call.expectations.result.back().rawBytes = bytes(1, uint8_t(false));
						else if (m_scanner.currentLiteral() == "nonempty")
							call.expectations.result.back().rawBytes = bytes(1, uint8_t(true));
						else
							throw TestParserError("Expected \"empty\" or \"nonempty\".");
						call.kind = FunctionCall::Kind::Storage;
						m_scanner.scanNextToken();
					}
					else
					{
						bool lowLevelCall = false;
						tie(call.signature, lowLevelCall) = parseFunctionSignature();
						if (lowLevelCall)
							call.kind = FunctionCall::Kind::LowLevel;

						if (accept(Token::Comma, true))
							call.value = parseFunctionCallValue();

						if (accept(Token::Colon, true))
							call.arguments = parseFunctionCallArguments();

						if (accept(Token::Newline, true))
						{
							call.displayMode = FunctionCall::DisplayMode::MultiLine;
							m_lineNumber++;
						}

						call.arguments.comment = parseComment();

						if (accept(Token::Newline, true))
						{
							call.displayMode = FunctionCall::DisplayMode::MultiLine;
							m_lineNumber++;
						}

						if (accept(Token::Arrow, true))
						{
							call.omitsArrow = false;
							call.expectations = parseFunctionCallExpectations();
							if (accept(Token::Newline, true))
								m_lineNumber++;
						}
						else
						{
							call.expectations.failure = false;
							call.displayMode = FunctionCall::DisplayMode::SingleLine;
						}

						call.expectations.comment = parseComment();

						if (call.signature == "constructor()")
							call.kind = FunctionCall::Kind::Constructor;
					}

					calls.emplace_back(std::move(call));
				}
				catch (TestParserError const& _e)
				{
					throw TestParserError("Line " + to_string(_lineOffset + m_lineNumber) + ": " + _e.what());
				}
			}
		}
	}
	return calls;
}

bool TestFileParser::accept(Token _token, bool const _expect)
{
	if (m_scanner.currentToken() != _token)
		return false;
	if (_expect)
		return expect(_token);
	return true;
}

bool TestFileParser::expect(Token _token, bool const _advance)
{
	if (m_scanner.currentToken() != _token || m_scanner.currentToken() == Token::Invalid)
		throw TestParserError(
			"Unexpected " + formatToken(m_scanner.currentToken()) + ": \"" +
			m_scanner.currentLiteral() + "\". " +
			"Expected \"" + formatToken(_token) + "\"."
			);
	if (_advance)
		m_scanner.scanNextToken();
	return true;
}

pair<string, bool> TestFileParser::parseFunctionSignature()
{
	string signature;
	bool hasName = false;

	if (accept(Token::Identifier, false))
	{
		hasName = true;
		signature = m_scanner.currentLiteral();
		expect(Token::Identifier);
	}

	signature += formatToken(Token::LParen);
	expect(Token::LParen);

	string parameters;
	if (!accept(Token::RParen, false))
		parameters = parseIdentifierOrTuple();

	while (accept(Token::Comma))
	{
		parameters += formatToken(Token::Comma);
		expect(Token::Comma);
		parameters += parseIdentifierOrTuple();
	}
	if (accept(Token::Arrow, true))
		throw TestParserError("Invalid signature detected: " + signature);

	if (!hasName && !parameters.empty())
		throw TestParserError("Signatures without a name cannot have parameters: " + signature);
	else
		signature += parameters;

	expect(Token::RParen);
	signature += formatToken(Token::RParen);

	return {signature, !hasName};
}

FunctionValue TestFileParser::parseFunctionCallValue()
{
	try
	{
		u256 value{ parseDecimalNumber() };
		Token token = m_scanner.currentToken();
		if (token != Token::Ether && token != Token::Wei)
			throw TestParserError("Invalid value unit provided. Coins can be wei or ether.");

		m_scanner.scanNextToken();

		FunctionValueUnit unit = token == Token::Wei ? FunctionValueUnit::Wei : FunctionValueUnit::Ether;
		return { (unit == FunctionValueUnit::Wei ? u256(1) : exp256(u256(10), u256(18))) * value, unit };
	}
	catch (std::exception const&)
	{
		throw TestParserError("Ether value encoding invalid.");
	}
}

FunctionCallArgs TestFileParser::parseFunctionCallArguments()
{
	FunctionCallArgs arguments;

	auto param = parseParameter();
	if (param.abiType.type == ABIType::None)
		throw TestParserError("No argument provided.");
	arguments.parameters.emplace_back(param);

	while (accept(Token::Comma, true))
		arguments.parameters.emplace_back(parseParameter());
	return arguments;
}

FunctionCallExpectations TestFileParser::parseFunctionCallExpectations()
{
	FunctionCallExpectations expectations;

	auto param = parseParameter();
	if (param.abiType.type == ABIType::None)
	{
		expectations.failure = false;
		return expectations;
	}
	expectations.result.emplace_back(param);

	if (param.abiType.type == ABIType::Failure)
	{
		expect(Token::Comma);
		expectations.result.emplace_back(parseParameter(true));
		return expectations;
	}

	while (accept(Token::Comma, true))
		expectations.result.emplace_back(parseParameter());

	/// We have always one virtual parameter in the parameter list.
	/// If its type is FAILURE, the expected result is also a REVERT etc.
	assert(expectations.result.at(0).abiType.type != ABIType::Failure);
	expectations.failure = false;
	return expectations;
}

string TestFileParser::refArgsToString(vector<pair<ParsedRefArgs, Token>> refargs) const {
	string result = "refargs {";
	bool first = true;;
	for (auto &p : refargs)
	{
		if (!first) result += ",";
		if (p.second == Token::Hex)
			result += "hex\"" + p.first.str + "\"";
		else if (p.second == Token::RefArgs)
			result += refArgsToString(p.first.refargs);
		else
			result += p.first.str;
		first = false;
	}
	result += "}";
	return result;
}

Parameter TestFileParser::parseParameter(bool isErrorMessage)
{
	Parameter parameter;
	if (accept(Token::Newline, true))
	{
		parameter.format.newline = true;
		m_lineNumber++;
	}
	parameter.abiType = ABIType{ABIType::None, ABIType::AlignNone, 0};

	bool isSigned = false;
	if (accept(Token::Left, true))
	{
		parameter.rawString += formatToken(Token::Left);
		expect(Token::LParen);
		parameter.rawString += formatToken(Token::LParen);
		parameter.alignment = Parameter::Alignment::Left;
	}
	if (accept(Token::Right, true))
	{
		parameter.rawString += formatToken(Token::Right);
		expect(Token::LParen);
		parameter.rawString += formatToken(Token::LParen);
		parameter.alignment = Parameter::Alignment::Right;
	}

	if (accept(Token::Sub, true))
	{
		parameter.rawString += formatToken(Token::Sub);
		isSigned = true;
	}
	if (accept(Token::Boolean))
	{
		if (isSigned)
			throw TestParserError("Invalid boolean literal.");

		string parsed = parseBoolean();
		parameter.rawBytes = BytesUtils::convertBoolean(parsed);
		parameter.rawString += parsed;
		parameter.abiType = ABIType{
			ABIType::Boolean, ABIType::AlignRight, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::HexNumber))
	{
		if (isSigned)
			throw TestParserError("Invalid hex number literal.");

		string parsed = parseHexNumber();
		parameter.rawString += parsed;
		parameter.rawBytes = BytesUtils::convertHexNumber(parsed);
		parameter.abiType = ABIType{
			ABIType::Hex, ABIType::AlignRight, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::Hex, true))
	{
		if (isSigned)
			throw TestParserError("Invalid hex string literal.");
		if (parameter.alignment != Parameter::Alignment::None)
			throw TestParserError("Hex string literals cannot be aligned or padded.");

		string parsed = parseString();
		parameter.rawString += "hex\"" + parsed + "\"";
		parameter.rawBytes = BytesUtils::convertHexNumber(parsed);
		parameter.abiType = ABIType{
			ABIType::HexString, ABIType::AlignNone, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::String))
	{
		if (isSigned)
			throw TestParserError("Invalid string literal.");
		if (parameter.alignment != Parameter::Alignment::None)
			throw TestParserError("String literals cannot be aligned or padded.");

		string parsed = parseString();
		parameter.rawString += "\"" + parsed + "\"";
		if (isErrorMessage)
			parameter.rawBytes = BytesUtils::convertErrorMessage(parsed);
		else
			parameter.rawBytes = BytesUtils::convertString(parsed);
		parameter.abiType = ABIType{
			ABIType::String, ABIType::AlignRight, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::Number))
	{
		auto type = isSigned ? ABIType::SignedDec : ABIType::UnsignedDec;

		string parsed = parseDecimalNumber();
		parameter.rawString += parsed;
		if (isSigned)
			parsed = "-" + parsed;

		parameter.rawBytes = BytesUtils::convertNumber(parsed);
		parameter.abiType = ABIType{type, ABIType::AlignRight, parameter.rawBytes.size()};
	}
	else if (accept(Token::Array))
	{
		if (isSigned)
			throw TestParserError("Invalid array literal.");
		if (parameter.alignment != Parameter::Alignment::None)
			throw TestParserError("Array literals cannot be aligned or padded.");

		vector<pair<string, Token>> parsed = parseArray(false);
		parameter.rawString += "array" + parsed[0].first + "[";
		bool first = true;;
		for (auto it = parsed.begin()+1; it != parsed.end(); ++it)
		{
			if (!first) parameter.rawString += ",";
			parameter.rawString += it->first;
			first = false;
		}
		parameter.rawString += "]";

		parameter.rawBytes = BytesUtils::convertArray(parsed, false);
		parameter.abiType = ABIType{
			ABIType::HexString, ABIType::AlignNone, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::DynArray))
	{
		if (isSigned)
			throw TestParserError("Invalid array literal.");
		if (parameter.alignment != Parameter::Alignment::None)
			throw TestParserError("Array literals cannot be aligned or padded.");

		vector<pair<string, Token>> parsed = parseArray(true);
		parameter.rawString += "array" + parsed[0].first + "[";
		bool first = true;;
		for (auto it = parsed.begin()+1; it != parsed.end(); ++it)
		{
			if (!first) parameter.rawString += ",";
			parameter.rawString += it->first;
			first = false;
		}
		parameter.rawString += "]";

		parameter.rawBytes = BytesUtils::convertArray(parsed, true);
		parameter.abiType = ABIType{
			ABIType::HexString, ABIType::AlignNone, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::RefArgs))
	{
		if (isSigned)
			throw TestParserError("Invalid refargs literal.");
		if (parameter.alignment != Parameter::Alignment::None)
			throw TestParserError("Refargs literals cannot be aligned or padded.");

		vector<pair<ParsedRefArgs, Token>> parsed = parseRefArgs();
		parameter.rawString += refArgsToString(parsed);

		parameter.rawBytes = BytesUtils::convertRefArgs(parsed);
		parameter.abiType = ABIType{
			ABIType::HexString, ABIType::AlignNone, parameter.rawBytes.size()
		};
	}
	else if (accept(Token::Failure, true))
	{
		if (isSigned)
			throw TestParserError("Invalid failure literal.");

		parameter.abiType = ABIType{ABIType::Failure, ABIType::AlignRight, 0};
		parameter.rawBytes = bytes{};
	}
	if (parameter.alignment != Parameter::Alignment::None)
	{
		expect(Token::RParen);
		parameter.rawString += formatToken(Token::RParen);
	}

	return parameter;
}

string TestFileParser::parseIdentifierOrTuple()
{
	string identOrTuple;

	auto parseArrayDimensions = [&]()
	{
		while (accept(Token::LBrack))
		{
			identOrTuple += formatToken(Token::LBrack);
			expect(Token::LBrack);
			if (accept(Token::Number))
				identOrTuple += parseDecimalNumber();
			identOrTuple += formatToken(Token::RBrack);
			expect(Token::RBrack);
		}
	};

	if (accept(Token::Identifier))
	{
		identOrTuple = m_scanner.currentLiteral();
		expect(Token::Identifier);
		parseArrayDimensions();
		return identOrTuple;
	}
	expect(Token::LParen);
	identOrTuple += formatToken(Token::LParen);
	identOrTuple += parseIdentifierOrTuple();

	while (accept(Token::Comma))
	{
		identOrTuple += formatToken(Token::Comma);
		expect(Token::Comma);
		identOrTuple += parseIdentifierOrTuple();
	}
	expect(Token::RParen);
	identOrTuple += formatToken(Token::RParen);

	parseArrayDimensions();
	return identOrTuple;
}

string TestFileParser::parseBoolean()
{
	string literal = m_scanner.currentLiteral();
	expect(Token::Boolean);
	return literal;
}

string TestFileParser::parseComment()
{
	string comment = m_scanner.currentLiteral();
	if (accept(Token::Comment, true))
		return comment;
	return string{};
}

string TestFileParser::parseDecimalNumber()
{
	string literal = m_scanner.currentLiteral();
	expect(Token::Number);
	return literal;
}

string TestFileParser::parseHexNumber()
{
	string literal = m_scanner.currentLiteral();
	expect(Token::HexNumber);
	return literal;
}

string TestFileParser::parseString()
{
	string literal = m_scanner.currentLiteral();
	expect(Token::String);
	return literal;
}

vector<pair<string, Token>> TestFileParser::parseArray(bool isDynamic)
{
	expect(isDynamic ? Token::DynArray : Token::Array);

	vector<pair<string, Token>> result;
	string elemsizeLiteral = m_scanner.currentLiteral();
	expect(Token::Number);
	result.push_back(make_pair(elemsizeLiteral, Token::Number));

	expect(Token::LBrack);
	bool first = true;;
	while (!accept(Token::RBrack))
	{
		if (!first) expect(Token::Comma);

		bool isSigned = false;
		if (accept(Token::Sub, true))
			isSigned = true;

		if (accept(Token::Boolean))
		{
			if (isSigned)
				throw TestParserError("Invalid boolean literal.");

			string literal = parseBoolean();
			result.push_back(make_pair(literal, Token::Boolean));
		}
		else if (accept(Token::Number))
		{
			string literal = parseDecimalNumber();
			if (isSigned)
				literal = "-" + literal;
			result.push_back(make_pair(literal, Token::Number));
		}
		else if (accept(Token::HexNumber))
		{
			if (isSigned)
				throw TestParserError("Invalid hex number literal.");

			string literal = parseHexNumber();
			result.push_back(make_pair(literal, Token::HexNumber));
		}
		else
			throw TestParserError("Invalid array literal");

		first = false;
	}
	expect(Token::RBrack);

	return result;
}

vector<pair<ParsedRefArgs, Token>> TestFileParser::parseRefArgs()
{
	expect(Token::RefArgs);

	vector<pair<ParsedRefArgs, Token>> result;
	expect(Token::LBrace);
	bool first = true;;
	while (!accept(Token::RBrace))
	{
		if (!first) expect(Token::Comma);

		bool isSigned = false;
		if (accept(Token::Sub, true))
			isSigned = true;

		if (accept(Token::Boolean))
		{
			if (isSigned)
				throw TestParserError("Invalid boolean literal.");

			string literal = parseBoolean();
			result.push_back(make_pair(ParsedRefArgs(literal), Token::Boolean));
		}
		else if (accept(Token::Number))
		{
			string literal = parseDecimalNumber();
			if (isSigned)
				literal = "-" + literal;
			result.push_back(make_pair(literal, Token::Number));
		}
		else if (accept(Token::HexNumber))
		{
			if (isSigned)
				throw TestParserError("Invalid hex number literal.");

			string literal = parseHexNumber();
			result.push_back(make_pair(literal, Token::HexNumber));
		}
		else if (accept(Token::Hex, true))
		{
			if (isSigned)
				throw TestParserError("Invalid hex string literal.");

			string literal = parseString();
			result.push_back(make_pair(literal, Token::Hex));
		}
		else if (accept(Token::String))
		{
			if (isSigned)
				throw TestParserError("Invalid string literal.");

			string literal = parseString();
			result.push_back(make_pair(literal, Token::String));
		}
		else if (accept(Token::RefArgs))
		{
			if (isSigned)
				throw TestParserError("Invalid string literal.");

			vector<pair<ParsedRefArgs, Token>> refargs = parseRefArgs();
			result.push_back(make_pair(refargs, Token::RefArgs));
		}
		else
			throw TestParserError("Invalid refargs literal");

		first = false;
	}
	expect(Token::RBrace);

	return result;
}

void TestFileParser::Scanner::readStream(istream& _stream)
{
	std::string line;
	while (std::getline(_stream, line))
		m_line += line;
	m_char = m_line.begin();
}

void TestFileParser::Scanner::scanNextToken()
{
	// Make code coverage happy.
	assert(formatToken(Token::NUM_TOKENS) == "");

	auto detectKeyword = [](std::string const& _literal = "") -> std::pair<Token, std::string> {
		if (_literal == "true") return {Token::Boolean, "true"};
		if (_literal == "false") return {Token::Boolean, "false"};
		if (_literal == "ether") return {Token::Ether, ""};
		if (_literal == "wei") return {Token::Wei, ""};
		if (_literal == "array") return {Token::Array, ""};
		if (_literal == "dynarray") return {Token::DynArray, ""};
		if (_literal == "left") return {Token::Left, ""};
		if (_literal == "library") return {Token::Library, ""};
		if (_literal == "refargs") return {Token::RefArgs, ""};
		if (_literal == "right") return {Token::Right, ""};
		if (_literal == "hex") return {Token::Hex, ""};
		if (_literal == "FAILURE") return {Token::Failure, ""};
		if (_literal == "storage") return {Token::Storage, ""};
		return {Token::Identifier, _literal};
	};

	auto selectToken = [this](Token _token, std::string const& _literal = "") {
		advance();
		m_currentToken = _token;
		m_currentLiteral = _literal;
	};

	m_currentToken = Token::Unknown;
	m_currentLiteral = "";
	do
	{
		switch(current())
		{
		case '/':
			advance();
			if (current() == '/')
				selectToken(Token::Newline);
			else
				selectToken(Token::Invalid);
			break;
		case '-':
			if (peek() == '>')
			{
				advance();
				selectToken(Token::Arrow);
			}
			else
				selectToken(Token::Sub);
			break;
		case ':':
			selectToken(Token::Colon);
			break;
		case '#':
			selectToken(Token::Comment, scanComment());
			break;
		case ',':
			selectToken(Token::Comma);
			break;
		case '(':
			selectToken(Token::LParen);
			break;
		case ')':
			selectToken(Token::RParen);
			break;
		case '[':
			selectToken(Token::LBrack);
			break;
		case ']':
			selectToken(Token::RBrack);
			break;
		case '{':
			selectToken(Token::LBrace);
			break;
		case '}':
			selectToken(Token::RBrace);
			break;
		case '\"':
			selectToken(Token::String, scanString());
			break;
		default:
			if (langutil::isIdentifierStart(current()))
			{
				std::tie(m_currentToken, m_currentLiteral) = detectKeyword(scanIdentifierOrKeyword());
				advance();
			}
			else if (langutil::isDecimalDigit(current()))
			{
				if (current() == '0' && peek() == 'x')
				{
					advance();
					advance();
					selectToken(Token::HexNumber, "0x" + scanHexNumber());
				}
				else
					selectToken(Token::Number, scanDecimalNumber());
			}
			else if (langutil::isWhiteSpace(current()))
				selectToken(Token::Whitespace);
			else if (isEndOfLine())
			{
				m_currentToken = Token::EOS;
				m_currentLiteral = "";
			}
			else
				throw TestParserError("Unexpected character: '" + string{current()} + "'");
			break;
		}
	}
	while (m_currentToken == Token::Whitespace);
}

string TestFileParser::Scanner::scanComment()
{
	string comment;
	advance();

	while (current() != '#')
	{
		comment += current();
		advance();
	}
	return comment;
}

string TestFileParser::Scanner::scanIdentifierOrKeyword()
{
	string identifier;
	identifier += current();
	while (langutil::isIdentifierPart(peek()))
	{
		advance();
		identifier += current();
	}
	return identifier;
}

string TestFileParser::Scanner::scanDecimalNumber()
{
	string number;
	number += current();
	while (langutil::isDecimalDigit(peek()))
	{
		advance();
		number += current();
	}
	return number;
}

string TestFileParser::Scanner::scanHexNumber()
{
	string number;
	number += current();
	while (langutil::isHexDigit(peek()))
	{
		advance();
		number += current();
	}
	return number;
}

string TestFileParser::Scanner::scanString()
{
	string str;
	advance();

	while (current() != '\"')
	{
		if (current() == '\\')
		{
			advance();
			switch (current())
			{
				case '\\':
					str += current();
					advance();
					break;
				case 'n':
					str += '\n';
					advance();
					break;
				case 'r':
					str += '\r';
					advance();
					break;
				case 't':
					str += '\t';
					advance();
					break;
				case '0':
					str += '\0';
					advance();
					break;
				case 'x':
					str += scanHexPart();
					break;
				default:
					throw TestParserError("Invalid or escape sequence found in string literal.");
			}
		}
		else
		{
			str += current();
			advance();
		}
	}
	return str;
}

// TODO: use fromHex() from CommonData
char TestFileParser::Scanner::scanHexPart()
{
	advance(); // skip 'x'

	int value{};
	if (isdigit(current()))
		value = current() - '0';
	else if (tolower(current()) >= 'a' && tolower(current()) <= 'f')
		value = tolower(current()) - 'a' + 10;
	else
		throw TestParserError("\\x used with no following hex digits.");

	advance();
	if (current() == '"')
		return static_cast<char>(value);

	value <<= 4;
	if (isdigit(current()))
		value |= current() - '0';
	else if (tolower(current()) >= 'a' && tolower(current()) <= 'f')
		value |= tolower(current()) - 'a' + 10;

	advance();

	return static_cast<char>(value);
}
