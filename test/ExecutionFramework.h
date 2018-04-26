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
 * Framework for executing contracts and testing them using RPC.
 */

#pragma once

#include <test/Options.h>
#include <test/RPCSession.h>

#include <libsolidity/interface/EVMVersion.h>

#include <libdevcore/FixedHash.h>
#include <libdevcore/SHA3.h>

#include <functional>

namespace dev
{
namespace test
{
	using rational = boost::rational<dev::bigint>;
	/// An Ethereum address: 20 bytes.
	/// @NOTE This is not endian-specific; it's just a bunch of bytes.
	using Address = h160;

        using s72 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<72, 72, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
	using u72 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<72, 72, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
	// The various denominations; here for ease of use where needed within code.
	static const u256 wei = 1;
	static const u256 shannon = u256("1000000000");
	static const u256 szabo = shannon * 1000;
	static const u256 finney = szabo * 1000;
	static const u256 ether = finney * 1000;

class ExecutionFramework
{

public:
	ExecutionFramework();

	virtual bytes const& compileAndRunWithoutCheck(
		std::string const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		std::vector<bytes> const& _arguments = std::vector<bytes>(),
		std::map<std::string, Address> const& _libraryAddresses = std::map<std::string, Address>()
	) = 0;

	bytes const& compileAndRun(
		std::string const& _sourceCode,
		u256 const& _value = 0,
		std::string const& _contractName = "",
		std::vector<bytes> const& _arguments = std::vector<bytes>(),
		std::map<std::string, Address> const& _libraryAddresses = std::map<std::string, Address>()
	)
	{
		bytes const& ret = compileAndRunWithoutCheck(_sourceCode, _value, _contractName, _arguments, _libraryAddresses);
		BOOST_REQUIRE(ret.size() != 0);
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

	std::vector<bytes> const& callContractFunctionWithValueNoEncoding(std::string _sig, u256 const& _value, std::vector<bytes> const& _arguments)
	{
		//TODO: handle overloading correctly
		FixedHash<4> hash(dev::keccak256(_sig));
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
			message += toHex(val) + " ";
		}
		message += "]\nActual:   [ ";
		for (bytes const& val : contractResult) {
			message += toHex(val) + " ";
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
				message += toHex(val) + " ";
			}
			message += "]\nActual:   [ ";
			for (bytes const& val : contractResult) {
				message += toHex(val) + " ";
			}
			BOOST_CHECK_MESSAGE(
				contractResult == cppResult,
				"Computed values do not match.\nContract: " +
					std::string("\nArgument: ") +
					toHex(encode(argument)) +
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

	static std::pair<bool, std::string> compareAndCreateMessage(std::vector<bytes> const& _result, std::vector<bytes> const& _expectation);

	static bytes encode(bool _value) { return encode(byte(_value)); }
	static bytes encode(int _value) { return encode(bigint(_value)); }
	static bytes encodeLog(int _value) { return encode(bigint(_value)); }
	static bytes encodeLog(int16_t _value) { return encode(dev::toBigEndian(_value)); }
	static bytes encodeLog(uint64_t _value) { return encode(dev::toBigEndian(_value)); }
	static bytes encode(size_t _value) { return encode(bigint(_value)); }
	static bytes encode(char const* _value) { return encode(std::string(_value)); }
	static bytes encode(byte _value) { return toBigEndian(_value); }
	static bytes encode(u160 const& _value) { return encode(bigint(_value)); }
	static bytes encodeLog(u160 const& _value) { return encode(dev::toBigEndian(_value)); }
	static bytes encodeLog(s72 const& _value) { bytes encoded(9); dev::toBigEndian(s2u(_value), encoded); return encoded; }
	static bytes encode(u256 const& _value) { return encode(bigint(_value)); }
	static bytes encodeLog(u256 const& _value) { return encode(dev::toBigEndian(_value)); }
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
	static bytes encode(h256 const& _value) {
		if (_value[0] > 0x7f)
			return bytes(1, 0) + _value.asBytes();
		else
			return _value.asBytes();
	}
	static bytes encode(bytes const& _value, bool _padLeft = true)
	{
		return _padLeft ? _value : _value;
	}
	static bytes encodeLog(bytes const& _value)
	{
		return _value;
	}
	static bytes encode(std::string const& _value) { return encode(asBytes(_value), false); }
        static bytes encodeLog(std::string const& _value) { return encode(_value); }
	template <class _T>
	static bytes encode(std::vector<_T> const& _value)
	{
		bytes ret;
		for (auto const& v: _value)
			ret += encode(v);
		return ret;
	}

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
				dev::toBigEndian(arg, encodedArg);
				encoded += encodedArg;
			} else {
				encoded += encodeLog(bigint(arg));
			}
		}
		while (encoded.size() >= 2 && encoded[0] == 0 && encoded[1] <= 0x7f) {
			encoded.erase(encoded.begin());
		}
		return encoded;
	}


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

	static bytes rlpEncodeLength(bytes const& _str, byte const& offset)
	{
		if (_str.size() <= 55) {
			return bytes(1, offset+_str.size()) + _str;
		}
		bytes len = toBigEndian(_str.size());
		return bytes(1, offset+55+len.size()) + len + _str;
	}

protected:
	void sendMessage(std::vector<bytes> const& _arguments, std::string _function, bytes const& _data, bool _isCreation, u256 const& _value = 0);
	void sendEther(Address const& _to, u256 const& _value);
	size_t currentTimestamp();
	size_t blockTimestamp(u256 _number);

	/// @returns the (potentially newly created) _ith address.
	Address account(size_t _i);

	u256 balanceAt(Address const& _addr);
	bool storageEmpty(Address const& _addr);
	bool addressHasCode(Address const& _addr);

	RPCSession& m_rpc;

	struct LogEntry
	{
		Address address;
		std::vector<h256> topics;
		bytes data;
	};

	solidity::EVMVersion m_evmVersion;
	unsigned m_optimizeRuns = 200;
	bool m_optimize = false;
	bool m_showMessages = false;
	Address m_sender;
	Address m_contractAddress;
	u256 m_blockNumber;
	u256 const m_gasPrice = 100 * szabo;
	u256 const m_gas = 100000000;
	std::vector<bytes> m_output;
	u256 m_status;
	std::vector<LogEntry> m_logs;
	u256 m_gasUsed;
};

#define ABI_CHECK(result, expectation) do { \
	auto abiCheckResult = ExecutionFramework::compareAndCreateMessage((result), (expectation)); \
	BOOST_CHECK_MESSAGE(abiCheckResult.first, abiCheckResult.second); \
} while (0)


}
} // end namespaces

