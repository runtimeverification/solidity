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
/** @file Common.h
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 *
 * Very common stuff (i.e. that every other header needs except vector_ref.h).
 */

#pragma once

// way too many unsigned to size_t warnings in 32 bit build
#ifdef _M_IX86
#pragma warning(disable:4244)
#endif

#if _MSC_VER && _MSC_VER < 1900
#define _ALLOW_KEYWORD_MACROS
#define noexcept throw()
#endif

#ifdef __INTEL_COMPILER
#pragma warning(disable:3682) //call through incomplete class
#endif

#include <libsolutil/vector_ref.h>

#include <boost/version.hpp>
#if (BOOST_VERSION < 106500)
#error "Unsupported Boost version. At least 1.65 required."
#endif

#include <boost/multiprecision/cpp_int.hpp>

#include <map>
#include <utility>
#include <vector>
#include <functional>
#include <string>

namespace solidity
{

// Binary data types.
using bytes = std::vector<uint8_t>;
using bytesRef = util::vector_ref<uint8_t>;
using bytesConstRef = util::vector_ref<uint8_t const>;

// Numeric types.
using bigint = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>>;
using u256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using s256 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<256, 256, boost::multiprecision::signed_magnitude, boost::multiprecision::unchecked, void>>;
using u160 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<160, 160, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using u24 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<24, 24, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;
using u248 = boost::multiprecision::number<boost::multiprecision::cpp_int_backend<248, 248, boost::multiprecision::unsigned_magnitude, boost::multiprecision::unchecked, void>>;

// Map types.
using StringMap = std::map<std::string, std::string>;

// String types.
using strings = std::vector<std::string>;

/// Interprets @a _u as a two's complement signed number and returns the resulting s256.
inline s256 u2s(u256 _u)
{
	static bigint const c_end = bigint(1) << 256;
	if (boost::multiprecision::bit_test(_u, 255))
		return s256(-(c_end - _u));
	else
		return s256(_u);
}

/// @returns the two's complement signed representation of the signed number _u.
inline u256 s2u(s256 _u)
{
	static bigint const c_end = bigint(1) << 256;
	if (_u >= 0)
		return u256(_u);
	else
		return u256(c_end + _u);
}

inline u256 exp256(u256 _base, u256 _exponent)
{
	using boost::multiprecision::limb_type;
	u256 result = 1;
	while (_exponent)
	{
		if (boost::multiprecision::bit_test(_exponent, 0))
			result *= _base;
		_base *= _base;
		_exponent >>= 1;
	}
	return result;
}

/// Checks whether _mantissa * (X ** _exp) fits into 4096 bits,
/// where X is given indirectly via _log2OfBase = log2(X).
bool fitsPrecisionBaseX(bigint const& _mantissa, double _log2OfBase, uint32_t _exp);

inline std::ostream& operator<<(std::ostream& os, bytes const& _bytes)
{
	std::ostringstream ss;
	ss << std::hex;
	std::copy(_bytes.begin(), _bytes.end(), std::ostream_iterator<int>(ss, ","));
	std::string result = ss.str();
	result.pop_back();
	os << "[" + result + "]";
	return os;
}

/// RAII utility class whose destructor calls a given function.
class ScopeGuard
{
public:
	explicit ScopeGuard(std::function<void(void)> _f): m_f(std::move(_f)) {}
	~ScopeGuard() { m_f(); }

private:
	std::function<void(void)> m_f;
};

}
