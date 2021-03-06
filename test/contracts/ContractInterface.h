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

#pragma once

#include <boost/test/unit_test.hpp>
#include <test/ExecutionFramework.h>

#include <functional>

namespace solidity::test
{

class ContractInterface
{
public:
	ContractInterface(ExecutionFramework& _framework): m_framework(_framework) {}

	void setNextValue(u256 const& _value) { m_nextValue = _value; }

protected:
	template <class... Args>
	std::vector<bytes> const& call(std::string const& _sig, Args const&... _arguments)
	{
		auto const& ret = m_framework.callContractFunctionWithValue(_sig, m_nextValue, _arguments...);
		m_nextValue = 0;
		return ret;
	}

	template <class... Args>
	bytes const& callReturning(std::string const& _sig, Args const&... _arguments)
	{
		auto const& ret = call(_sig, _arguments...);
		BOOST_REQUIRE(ret.size() == 1);
		return ret[0];
	}

	void callString(std::string const& _name, std::string const& _arg)
	{
		BOOST_CHECK(call(_name + "(string)", m_framework.encodeDyn(_arg)).empty());
	}

	void callStringAddress(std::string const& _name, std::string const& _arg1, util::h160 const& _arg2)
	{
		BOOST_CHECK(call(_name + "(string,address)", m_framework.encodeDyn(_arg1), _arg2).empty());
	}

	void callStringAddressBool(std::string const& _name, std::string const& _arg1, util::h160 const& _arg2, bool _arg3)
	{
		BOOST_CHECK(call(_name + "(string,address,bool)", m_framework.encodeDyn(_arg1), _arg2, _arg3).empty());
	}

	void callStringBytes32(std::string const& _name, std::string const& _arg1, util::h256 const& _arg2)
	{
		BOOST_CHECK(call(_name + "(string,bytes32)", m_framework.encodeDyn(_arg1), _arg2).empty());
	}

	util::h160 callStringReturnsAddress(std::string const& _name, std::string const& _arg)
	{
<<<<<<< ours
		bytes const& ret = callReturning(_name + "(string)", m_framework.encodeDyn(_arg));
		return u160(u256(h256(ret, h256::AlignRight)));
=======
		bytes const& ret = call(_name + "(string)", u256(0x20), _arg.length(), _arg);
		BOOST_REQUIRE(ret.size() == 0x20);
		BOOST_CHECK(std::count(ret.begin(), ret.begin() + 12, 0) == 12);
		bytes const addr{ret.begin() + 12, ret.end()};
		return util::h160(addr);
>>>>>>> theirs
	}

	std::string callAddressReturnsString(std::string const& _name, util::h160 const& _arg)
	{
<<<<<<< ours
		bytesConstRef const ret(&callReturning(_name + "(address)", _arg));
		u256 len(h256(ret.cropped(ret.size() - 8, 0x08), h256::AlignRight));
		return ret.cropped(0x00, size_t(len)).toString();
=======
		bytesConstRef const ret(&call(_name + "(address)", _arg));
		BOOST_REQUIRE(ret.size() >= 0x40);
		u256 offset(util::h256(ret.cropped(0, 0x20)));
		BOOST_REQUIRE_EQUAL(offset, 0x20);
		u256 len(util::h256(ret.cropped(0x20, 0x20)));
		BOOST_REQUIRE_EQUAL(ret.size(), 0x40 + ((len + 0x1f) / 0x20) * 0x20);
		return ret.cropped(0x40, size_t(len)).toString();
>>>>>>> theirs
	}

	util::h256 callStringReturnsBytes32(std::string const& _name, std::string const& _arg)
	{
<<<<<<< ours
		bytes const& ret = callReturning(_name + "(string)", m_framework.encodeDyn(_arg));
		return h256(ret, h256::AlignRight);
	}

	u256 callVoidReturnsUInt256(std::string const& _name)
	{
		bytes const& ret = callReturning(_name + "()");
		BOOST_REQUIRE(ret.size() > 0 && ret.size() <= 32);
		return fromBigEndian<u256>(ret);
	}

	u256 callAddressReturnsUInt256(std::string const& _name, u160 const& _arg)
	{
		bytes const& ret = callReturning(_name + "(address)", _arg);
		BOOST_REQUIRE(ret.size() > 0 && ret.size() <= 32);
		return fromBigEndian<u256>(ret);
	}

	bool callAddressUInt256ReturnsBool(std::string const& _name, u160 const& _arg1, u256 const& _arg2)
	{
		bytes const& ret = callReturning(_name + "(address,uint256)", _arg1, _arg2);
		BOOST_REQUIRE(ret.size() == 1);
		return ret[0];
	}

	bool callAddressAddressUInt256ReturnsBool(std::string const& _name, u160 const& _arg1, u160 const& _arg2, u256 const& _arg3)
	{
		bytes const& ret = callReturning(_name + "(address,address,uint256)", _arg1, _arg2, _arg3);
		BOOST_REQUIRE(ret.size() == 1);
		return ret[0];
	}

	u256 callAddressAddressReturnsUInt256(std::string const& _name, u160 const& _arg1, u160 const& _arg2)
	{
		bytes const& ret = callReturning(_name + "(address,address)", _arg1, _arg2);
		BOOST_REQUIRE(ret.size() > 0 && ret.size() <= 32);
		return fromBigEndian<u256>(ret);
=======
		bytes const& ret = call(_name + "(string)", u256(0x20), _arg.length(), _arg);
		BOOST_REQUIRE(ret.size() == 0x20);
		return util::h256(ret);
>>>>>>> theirs
	}

private:
	u256 m_nextValue;
	ExecutionFramework& m_framework;
};

} // end namespaces

