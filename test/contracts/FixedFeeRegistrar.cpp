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
 * Tests for a fixed fee registrar contract.
 */

#include <libsolutil/LazyInit.h>

#include <string>
#include <tuple>
#include <optional>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4535) // calling _set_se_translator requires /EHa
#endif
#include <boost/test/unit_test.hpp>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <test/libsolidity/SolidityExecutionFramework.h>

using namespace std;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::test;

namespace solidity::frontend::test
{

namespace
{

static char const* registrarCode = R"DELIMITER(
//sol FixedFeeRegistrar
// Simple global registrar with fixed-fee reservations.
// @authors:
//   Gav Wood <g@ethdev.com>

pragma solidity >=0.4.0 <0.9.0;

abstract contract Registrar {
	event Changed(string indexed name);

	function owner(string memory _name) public virtual view returns (address o_owner);
	function addr(string memory _name) public virtual view returns (address o_address);
	function subRegistrar(string memory _name) virtual public view returns (address o_subRegistrar);
	function content(string memory _name) public virtual view returns (bytes32 o_content);
}

contract FixedFeeRegistrar is Registrar {
	struct Record {
		address addr;
		address subRegistrar;
		bytes32 content;
		address owner;
	}

	modifier onlyrecordowner(string memory _name) { if (m_record(_name).owner == msg.sender) _; }

	function reserve(string memory _name) public payable {
		Record storage rec = m_record(_name);
		if (rec.owner == 0x0000000000000000000000000000000000000000 && msg.value >= c_fee) {
			rec.owner = msg.sender;
			emit Changed(_name);
		}
	}
	function disown(string memory _name, address payable _refund) onlyrecordowner(_name) public {
		delete m_recordData[uint(uint256(keccak256(bytes(_name)))) / 8];
		if (!_refund.send(c_fee))
			revert();
		emit Changed(_name);
	}
	function transfer(string memory _name, address _newOwner) onlyrecordowner(_name) public {
		m_record(_name).owner = _newOwner;
		emit Changed(_name);
	}
	function setAddr(string memory _name, address _a) onlyrecordowner(_name) public {
		m_record(_name).addr = _a;
		emit Changed(_name);
	}
	function setSubRegistrar(string memory _name, address _registrar) onlyrecordowner(_name) public {
		m_record(_name).subRegistrar = _registrar;
		emit Changed(_name);
	}
	function setContent(string memory _name, bytes32 _content) onlyrecordowner(_name) public {
		m_record(_name).content = _content;
		emit Changed(_name);
	}

	function record(string memory _name) public view returns (address o_addr, address o_subRegistrar, bytes32 o_content, address o_owner) {
		Record storage rec = m_record(_name);
		o_addr = rec.addr;
		o_subRegistrar = rec.subRegistrar;
		o_content = rec.content;
		o_owner = rec.owner;
	}
	function addr(string memory _name) public override view returns (address) { return m_record(_name).addr; }
	function subRegistrar(string memory _name) public override view returns (address) { return m_record(_name).subRegistrar; }
	function content(string memory _name) public override view returns (bytes32) { return m_record(_name).content; }
	function owner(string memory _name) public override view returns (address) { return m_record(_name).owner; }

	Record[2**253] m_recordData;
	function m_record(string memory _name) view internal returns (Record storage o_record) {
		return m_recordData[uint(uint256(keccak256(bytes(_name)))) / 8];
	}
	uint constant c_fee = 5 gwei;
}
)DELIMITER";

static LazyInit<bytes> s_compiledRegistrar;

class RegistrarTestFramework: public SolidityExecutionFramework
{
protected:
	void deployRegistrar()
	{
		bytes const& compiled = s_compiledRegistrar.init([&]{
			return compileContract(registrarCode, "FixedFeeRegistrar");
		});

		sendMessage(std::vector<bytes>(), "", compiled, true);
		BOOST_REQUIRE(m_status == 0);
	}
	u256 const m_fee = u256("5000000000");
};

}

/// This is a test suite that tests optimised code!
BOOST_FIXTURE_TEST_SUITE(SolidityFixedFeeRegistrar, RegistrarTestFramework)

BOOST_AUTO_TEST_CASE(creation)
{
	deployRegistrar();
}

BOOST_AUTO_TEST_CASE(reserve)
{
	// Test that reserving works and fee is taken into account.
	deployRegistrar();
	string name[] = {"abc", "def", "ghi"};
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee, encodeDyn(name[0])) == encodeArgs());
	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name[0])) == encodeArgs(u160(account(0))));
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee + 1, encodeDyn(name[1])) == encodeArgs());
	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name[1])) == encodeArgs(u160(account(0))));
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee - 1, encodeDyn(name[2])) == encodeArgs());
	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name[2])) == encodeArgs(0));
}

BOOST_AUTO_TEST_CASE(double_reserve)
{
	// Test that it is not possible to re-reserve from a different address.
	deployRegistrar();
	string name = "abc";
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee, encodeDyn(name)) == encodeArgs());
	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name)) == encodeArgs(u160(account(0))));

	sendEther(account(1), 10 * u256("1000000000"));
	m_sender = account(1);
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee, encodeDyn(name)) == encodeArgs());
	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name)) == encodeArgs(u160(account(0))));
}

BOOST_AUTO_TEST_CASE(properties)
{
	// Test setting and retrieving  the various properties works.
	deployRegistrar();
	string names[] = {"abc", "def", "ghi"};
	size_t addr = 0x9872543;
	size_t count = 1;
	for (string const& name: names)
	{
		addr++;
		m_sender = account(0);
		sendEther(account(count), 5 * u256("1000000000"));
		m_sender = account(count);
		u160 owner = u160(m_sender);
		// setting by sender works
		BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee, encodeDyn(name)) == encodeArgs());
		BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name)) == encodeArgs(owner));
		BOOST_CHECK(callContractFunction("setAddr(string,address)", encodeDyn(name), u256(addr)) == encodeArgs());
		BOOST_CHECK(callContractFunction("addr(string)", encodeDyn(name)) == encodeArgs(addr));
		BOOST_CHECK(callContractFunction("setSubRegistrar(string,address)", encodeDyn(name), addr + 20) == encodeArgs());
		BOOST_CHECK(callContractFunction("subRegistrar(string)", encodeDyn(name)) == encodeArgs(addr + 20));
		BOOST_CHECK(callContractFunction("setContent(string,bytes32)", encodeDyn(name), addr + 90) == encodeArgs());
		BOOST_CHECK(callContractFunction("content(string)", encodeDyn(name)) == encodeArgs(addr + 90));
		count++;
		// but not by someone else
		m_sender = account(0);
		sendEther(account(count), 5 * u256("1000000000"));
		m_sender = account(count);
		BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name)) == encodeArgs(owner));
		BOOST_CHECK(callContractFunction("setAddr(string,address)", encodeDyn(name), addr + 1) == encodeArgs());
		BOOST_CHECK(callContractFunction("addr(string)", encodeDyn(name)) == encodeArgs(addr));
		BOOST_CHECK(callContractFunction("setSubRegistrar(string,address)", encodeDyn(name), addr + 20 + 1) == encodeArgs());
		BOOST_CHECK(callContractFunction("subRegistrar(string)", encodeDyn(name)) == encodeArgs(addr + 20));
		BOOST_CHECK(callContractFunction("setContent(string,bytes32)", encodeDyn(name), addr + 90 + 1) == encodeArgs());
		BOOST_CHECK(callContractFunction("content(string)", encodeDyn(name)) == encodeArgs(addr + 90));
		count++;
	}
}

BOOST_AUTO_TEST_CASE(transfer)
{
	deployRegistrar();
	string name = "abc";
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee, encodeDyn(name)) == encodeArgs());
	BOOST_CHECK(callContractFunction("setContent(string,bytes32)", encodeDyn(name), account(0)) == encodeArgs());
	BOOST_CHECK(callContractFunction("transfer(string,address)", encodeDyn(name), u256(555)) == encodeArgs());
	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name)) == encodeArgs(u256(555)));
	BOOST_CHECK(callContractFunction("content(string)", encodeDyn(name)) == encodeArgs(u160(account(0))));
}

BOOST_AUTO_TEST_CASE(disown)
{
	deployRegistrar();
	string name = "abc";
	BOOST_REQUIRE(callContractFunctionWithValue("reserve(string)", m_fee, encodeDyn(name)) == encodeArgs());
	BOOST_CHECK(callContractFunction("setContent(string,bytes32)", encodeDyn(name), account(0)) == encodeArgs());
	BOOST_CHECK(callContractFunction("setAddr(string,address)", encodeDyn(name), u256(124)) == encodeArgs());
	BOOST_CHECK(callContractFunction("setSubRegistrar(string,address)", encodeDyn(name), u256(125)) == encodeArgs());

	BOOST_CHECK_EQUAL(balanceAt(h160(0x124)), 0);
	BOOST_CHECK(callContractFunction("disown(string,address)", encodeDyn(name), u256(0x124)) == encodeArgs());
	BOOST_CHECK_EQUAL(balanceAt(h160(0x124)), m_fee);

	BOOST_CHECK(callContractFunction("owner(string)", encodeDyn(name)) == encodeArgs(u256(0)));
	BOOST_CHECK(callContractFunction("content(string)", encodeDyn(name)) == encodeArgs(u256(0)));
	BOOST_CHECK(callContractFunction("addr(string)", encodeDyn(name)) == encodeArgs(u256(0)));
	BOOST_CHECK(callContractFunction("subRegistrar(string)", encodeDyn(name)) == encodeArgs(u256(0)));
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespaces
