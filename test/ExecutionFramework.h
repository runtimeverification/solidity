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
 * @date 2014
 * Framework for executing contracts and testing them using RPC.
 */

#pragma once

#include <test/Common.h>
#include <test/EVMHost.h>
#include <test/RPCSession.h>

#include <libsolidity/interface/OptimiserSettings.h>
#include <libsolidity/interface/DebugSettings.h>

#include <liblangutil/EVMVersion.h>

#include <libsolutil/FixedHash.h>
#include <libsolutil/Keccak256.h>
#include <libsolutil/ErrorCodes.h>

#include <functional>

#include <boost/test/unit_test.hpp>

namespace solidity::test
{
using rational = boost::rational<bigint>;

using s72 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<72, 72, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using u72 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<72, 72, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;

// The ether and gwei denominations; here for ease of use where needed within code.
static const u256 gwei = u256(1) << 9;
static const u256 ether = u256(1) << 18;

class ExecutionFramework
{

public:
	ExecutionFramework();
	ExecutionFramework(langutil::EVMVersion _evmVersion, std::vector<boost::filesystem::path> const& _vmPaths);
	virtual ~ExecutionFramework() = default;

	virtual bytes const& compileAndRunWithoutCheck(
		std::map<std::string, std::string> const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		bool compileFail = false,
		std::vector<bytes> const& _arguments = {},
		std::map<std::string, util::h160> const& _libraryAddresses = {}
	) = 0;

	bytes const& compileAndRun(
		std::string const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		std::vector<bytes> const& _arguments = {},
		std::map<std::string, util::h160> const& _libraryAddresses = {}
	)
	{
		bytes const& ret = compileAndRunWithoutCheck(
			{{"", _sourceCode}},
			_value,
			_contractName,
			false,
			_arguments,
			_libraryAddresses
		);
		BOOST_REQUIRE(ret.size() != 0);
		BOOST_REQUIRE(m_status == 0);
		return ret;
	}

	std::vector<bytes> const& callFallbackWithValue(u256 const& _value)
	{
		sendMessage(std::vector<bytes>(), "deposit", bytes(), false, _value);
		return m_output;
	}

	std::vector<bytes> const & callFallback()
	{
		return callFallbackWithValue(0);
	}

	std::vector<bytes> const& callLowLevel(bytes const& _data, u256 const& _value)
	{
		if (_data.size() > 0)
		{
			sendMessage(std::vector<bytes>(1, _data), "deposit", bytes(), false, _value);
		}
		else
		{
			sendMessage(std::vector<bytes>(), "deposit", bytes(), false, _value);
		}
		return m_output;
	}

	std::vector<bytes> const& callContractFunctionWithValueNoEncoding(std::string _sig, u256 const& _value, std::vector<bytes> const& _arguments)
	{
		//TODO: handle overloading correctly
		util::FixedHash<4> hash(util::keccak256(_sig));
		sendMessage(_arguments, _sig, bytes(), false, _value);
		return m_output;
	}

	std::vector<bytes> const& callContractFunctionNoEncoding(std::string _sig, std::vector<bytes> const& _arguments)
	{
		return callContractFunctionWithValueNoEncoding(_sig, 0, _arguments);
	}

	template <class... Args>
	std::vector<bytes> const& callContractFunctionWithValue(std::string _sig, u256 const& _value, Args const&... _arguments)
	{
		return callContractFunctionWithValueNoEncoding(_sig, _value, encodeArgs(_arguments...));
	}

	template <class... Args>
	std::vector<bytes> const& callContractFunction(std::string _sig, Args const&... _arguments)
	{
		return callContractFunctionWithValue(_sig, 0, _arguments...);
	}

	template <class CppFunction, class... Args>
	void testContractAgainstCpp(std::string _sig, CppFunction const& _cppFunction, Args const&... _arguments)
	{
		std::vector<bytes> contractResult = callContractFunction(_sig, _arguments...);
		std::vector<bytes> cppResult = callCppAndEncodeResult(_cppFunction, _arguments...);
		std::string message = "\nExpected: [ ";

		for (bytes const& val : cppResult) {
			message += util::toHex(val) + " ";
		}
		message += "]\nActual:   [ ";
		for (bytes const& val : contractResult) {
			message += util::toHex(val) + " ";
		}
		BOOST_CHECK_MESSAGE(
			contractResult == cppResult,
			"Computed values do not match.\nContract: " +
				message + "]\n"
		);
	}

	template <class CppFunction, class... Args>
	void testContractAgainstCppOnRange(std::string _sig, CppFunction const& _cppFunction, u256 const& _rangeStart, u256 const& _rangeEnd)
	{
		for (u256 argument = _rangeStart; argument < _rangeEnd; ++argument)
		{
			std::vector<bytes> contractResult = callContractFunction(_sig, argument);
			std::vector<bytes> cppResult = callCppAndEncodeResult(_cppFunction, argument);
			std::string message = "\nExpected: [ ";

			for (bytes const& val : cppResult) {
				message += util::toHex(val) + " ";
			}
			message += "]\nActual:   [ ";
			for (bytes const& val : contractResult) {
				message += util::toHex(val) + " ";
			}
			BOOST_CHECK_MESSAGE(
				contractResult == cppResult,
				"Computed values do not match.\nContract: " +
					std::string("\nArgument: ") +
					util::toHex(encode(argument)) +
					message + "]\n"
			);
		}
	}

	static inline u72 s2u(s72 _u)
	{
		static const bigint c_end = bigint(1) << 72;
	    if (_u >= 0)
			return u72(_u);
	    else
			return u72(c_end + _u);
	}

	static std::pair<bool, std::string> compareAndCreateMessage(bytes const& _result, bytes const& _expectation);
	static std::pair<bool, std::string> compareAndCreateMessage(std::vector<bytes> const& _result, std::vector<bytes> const& _expectation);

	static bytes encode(bool _value) { return encode(uint8_t(_value)); }
	static bytes encode(int _value) { return encode(bigint(_value)); }
	static bytes encodeLog(int _value) { return encode(bigint(_value)); }
	static bytes encodeLog(int16_t _value) { return encode(util::toBigEndian(_value)); }
	static bytes encodeLog(uint64_t _value) { return encode(util::toBigEndian(_value)); }
	static bytes encode(size_t _value) { return encode(bigint(_value)); }
	static bytes encode(char const* _value) { return encode(std::string(_value)); }
	static bytes encode(uint8_t _value) { return toBigEndian(_value); }
	static bytes encode(u160 const& _value) { return encode(bigint(_value)); }
	static bytes encodeLog(u160 const& _value) { return encode(util::toBigEndian(_value)); }
	static bytes encodeLog(s72 const& _value) { bytes encoded(9); util::toBigEndian(s2u(_value), encoded); return encoded; }
	static bytes encode(u256 const& _value) { return encode(bigint(_value)); }
	static bytes encodeLog(u256 const& _value) { return encode(util::toBigEndian(_value)); }
        static bytes encode(bigint const& _value) { return toBigEndian(_value); }
        static bytes encodeLog(bigint const& _value) { bytes encoded = toBigEndian(_value); return encoded + encodeLog(uint64_t(encoded.size())); }
	/// @returns the fixed-point encoding of a rational number with a given
	/// number of fractional bits.
	static bytes encode(std::pair<rational, int> const& _valueAndPrecision)
	{
		rational const& value = _valueAndPrecision.first;
		int fractionalBits = _valueAndPrecision.second;
		return encode(u256((value.numerator() << fractionalBits) / value.denominator()));
	}
	static bytes encode(util::h256 const& _value) {
		if (_value[0] > 0x7f)
			return bytes(1, 0) + _value.asBytes();
		else
			return _value.asBytes();
	}
	static bytes encode(util::h160 const& _value) { return encode(util::h256(_value, util::h256::AlignRight)); }
	static bytes encode(bytes const& _value, bool _padLeft = true)
	{
		return _padLeft ? _value : _value;
	}
	static bytes encodeLog(bytes const& _value)
	{
		return _value;
	}
	static bytes encode(std::string const& _value) { return encode(util::asBytes(_value), false); }
    static bytes encodeLog(std::string const& _value) { return encode(_value); }
	template <class T>
	static bytes encode(std::vector<T> const& _value)
	{
		bytes ret;
		for (auto const& v: _value)
			ret += encode(v);
		return ret;
	}
	template <class T>
	static bytes encodeLog(std::vector<T> const& _value) { return encode(_value); }

	template <class FirstArg, class... Args>
	static std::vector<bytes> encodeArgs(FirstArg const& _firstArg, Args const&... _followingArgs)
	{
		return std::vector<bytes>(1, encode(_firstArg)) + encodeArgs(_followingArgs...);
	}
	static std::vector<bytes> encodeArgs()
	{
		return std::vector<bytes>();
	}
	template <class FirstArg, class... Args>
	static bytes encodeLogs(FirstArg const& _firstArg, Args const&... _followingArgs)
	{
		bytes encoded = encodeLog(_firstArg);
		std::reverse(encoded.begin(), encoded.end());
		return encoded + encodeLogs(_followingArgs...);
	}
	static bytes encodeLogs()
	{
		return bytes();
	}
	template <class FirstArg, class... Args>
	static bytes encodeRefArgs(FirstArg const& _firstArg, Args const&... _followingArgs)
	{
		bytes encoded = encodeLog(_firstArg);
		return encodeRefArgs(_followingArgs...) + encoded;
	}
	static bytes encodeRefArgs()
	{
		return bytes();
	}

	static bytes encodeRefArray(std::vector<u256> args, size_t len, size_t size) {
		return encodeRefArray(args, size) + encodeRefArgs(1, int(len));
	}

	static bytes encodeRefArray(std::vector<u256> args, size_t size) {
		bytes encoded;
		std::reverse(args.begin(), args.end());
		for (u256 arg : args) {
			if (size > 0) {
				bytes encodedArg(size);
				util::toBigEndian(arg, encodedArg);
				encoded += encodedArg;
			} else {
				encoded += encodeLog(bigint(arg));
			}
		}
		while (encoded.size() >= 2 && encoded[0] == 0 && encoded[1] <= 0x7f) {
			encoded.erase(encoded.begin());
		}
		while (encoded.size() >= 2 && encoded[0] == 0xff && encoded[1] >= 0x80) {
			encoded.erase(encoded.begin());
		}
		return encoded;
	}

	/// @returns error returndata corresponding to the Panic(uint256) error code,
	/// if REVERT is supported by the current EVM version and the empty string otherwise.
	bytes panicData(util::PanicCode _code);

	//@todo might be extended in the future
	template <class Arg>
	static bytes encodeDyn(Arg const& _arg)
	{
		bytes encoded = encodeRefArgs(uint64_t(_arg.size()), _arg);
		while (encoded.size() >= 2 && encoded[0] == 0 && encoded[1] <= 0x7f) {
			encoded.erase(encoded.begin());
		}
		return encoded;
	}

	static bytes rlpEncode(bytes const& _first, std::vector<bytes> const& _second)
	{
		return rlpEncodeLength(rlpEncode(_first) + rlpEncode(_second), 0xc0);
	}
		
	static bytes rlpEncode(std::vector<bytes> const& _list)
	{
		bytes ret;
		for (bytes item : _list) {
			ret += rlpEncode(item);
		}
		return rlpEncodeLength(ret, 0xc0);
	}

	static bytes rlpEncode(bytes const& _str)
	{
		if (_str.size() == 1 && _str[0] <= 0x7f) {
			return _str;
		}
		return rlpEncodeLength(_str, 0x80);
	}

	static bytes toBigEndian(size_t _val, bool _partial = false)
	{
		if (_val == 0 && _partial) {
			return bytes();
		} else if (_val == 0) {
			return bytes(1, 0);
		}
		return toBigEndian(_val / 256, _val % 256 < 128) + bytes(1, _val % 256);
	}

	static bytes toBigEndian(bigint _val)
	{
		if (_val >= 0)
			return toBigEndian(_val, true);
		bigint representation = -_val * 2 - 1;
		unsigned int numBytes = 0;
		while (representation > 0) {
			representation >>= 8;
			numBytes++;
		}
		bigint twos = _val % (bigint(1) << (numBytes * 8));
		if (twos < 0) {
			twos += (bigint(1) << (numBytes * 8));
		}
		return toBigEndian(twos, false);
	}

	static bytes toBigEndian(bigint _val, bool _positive) {
		if (!_positive && _val < 256) {
			return bytes(1, (unsigned char)_val);
		} else if (_val == 0) {
			return bytes(1, 0);
		} else if (_positive && _val < 128) {
			return bytes(1, (unsigned char)_val);
		} else if (_val < 256) {
			return bytes{0, (unsigned char)_val};
		}
		return toBigEndian(_val / 256, _positive) + bytes(1, (unsigned char)_val % 256);

	}

	void modifyTimestamp(size_t timestamp) {
		m_timestamp = timestamp;
	}

	u256 gasLimit() const;
	u256 gasPrice() const;
	u256 blockHash(u256 const& _blockNumber) const;
	u256 blockNumber() const;

	template<typename Range>
	static bytes encodeArray(bool _dynamicallySized, bool _dynamicallyEncoded, Range const& _elements)
	{
		bytes result;
		if (_dynamicallySized)
			result += encode(u256(_elements.size()));
		if (_dynamicallyEncoded)
		{
			u256 offset = u256(_elements.size()) * 32;
			std::vector<bytes> subEncodings;
			for (auto const& element: _elements)
			{
				result += encode(offset);
				subEncodings.emplace_back(encode(element));
				offset += subEncodings.back().size();
			}
			for (auto const& subEncoding: subEncodings)
				result += subEncoding;
		}
		else
			for (auto const& element: _elements)
				result += encode(element);
		return result;
	}

private:
	template <class CppFunction, class... Args>
	auto callCppAndEncodeResult(CppFunction const& _cppFunction, Args const&... _arguments)
	-> typename std::enable_if<std::is_void<decltype(_cppFunction(_arguments...))>::value, std::vector<bytes>>::type
	{
		_cppFunction(_arguments...);
		return std::vector<bytes>();
	}
	template <class CppFunction, class... Args>
	auto callCppAndEncodeResult(CppFunction const& _cppFunction, Args const&... _arguments)
	-> typename std::enable_if<!std::is_void<decltype(_cppFunction(_arguments...))>::value, std::vector<bytes>>::type
	{
		return encodeArgs(_cppFunction(_arguments...));
	}

	static bytes rlpEncodeLength(bytes const& _str, uint8_t const& offset)
	{
		if (_str.size() <= 55) {
			return bytes(1, offset+_str.size()) + _str;
		}
		bytes len = toBigEndian(_str.size());
		return bytes(1, offset+55+len.size()) + len + _str;
	}

protected:
	void selectVM(evmc_capabilities _cap = evmc_capabilities::EVMC_CAPABILITY_EVM1);
	void reset();

	void sendMessage(std::vector<bytes> const& _arguments, std::string _function, bytes const& _data, bool _isCreation, u256 const& _value = 0);
	void sendEther(util::h160 const& _to, u256 const& _value);
	size_t currentTimestamp();
	size_t blockTimestamp(u256 _number);

	/// @returns the (potentially newly created) _ith address.
	util::h160 account(size_t _i);

	u256 balanceAt(util::h160 const& _addr);
	bool storageEmpty(util::h160 const& _addr);
	bool addressHasCode(util::h160 const& _addr);

	RPCSession& m_rpc;

    struct LogEntry
    {
            util::h160 address;
            std::vector<util::h256> topics;
            bytes data;
    };

	size_t numLogs() const;
	size_t numLogTopics(size_t _logIdx) const;
	util::h256 logTopic(size_t _logIdx, size_t _topicIdx) const;
	util::h160 logAddress(size_t _logIdx) const;
	bytes logData(size_t _logIdx) const;

	langutil::EVMVersion m_evmVersion;
	solidity::frontend::RevertStrings m_revertStrings = solidity::frontend::RevertStrings::Default;
	solidity::frontend::OptimiserSettings m_optimiserSettings = solidity::frontend::OptimiserSettings::minimal();
	bool m_showMessages = false;
	bool m_supportsEwasm = false;
	std::unique_ptr<EVMHost> m_evmcHost;

	std::vector<boost::filesystem::path> m_vmPaths;

	bool m_transactionSuccessful = true;
	util::h160 m_sender = util::h160(m_rpc.account(0));
	util::h160 m_contractAddress;
	u256 m_blockNumber;
	u256 const m_gasPrice = 10 * gwei;
	u256 const m_gas = 100000000;
	std::vector<bytes> m_output;
	bigint m_status;
    std::vector<LogEntry> m_logs;
	u256 m_gasUsed;
	size_t m_timestamp = 0;
};

#define ABI_CHECK(result, expectation) do { \
	auto abiCheckResult = ExecutionFramework::compareAndCreateMessage((result), (expectation)); \
	BOOST_CHECK_MESSAGE(abiCheckResult.first, abiCheckResult.second); \
} while (0)


} // end namespaces
