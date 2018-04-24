#include <boost/test/unit_test.hpp>
#include <test/libsolidity/SolidityExecutionFramework.h>
#include <test/contracts/ContractInterface.h>

using namespace std;
using namespace dev::test;

namespace dev {
namespace solidity {
namespace test {

namespace {

static char const* ERC20Code = R"DELIMITER(
pragma solidity ^0.4.0;

contract Token {
	event Transfer(address indexed _from, address indexed _to, uint256 _value);
	event Approval(address indexed _owner, address indexed _spender, uint256 _value);

	function totalSupply() constant public returns (uint256 supply);
	function balanceOf(address _owner) constant public returns (uint256 balance);
	function transfer(address _to, uint256 _value) public returns (bool success);
	function transferFrom(address _from, address _to, uint256 _value) public returns (bool success);
	function approve(address _spender, uint256 _value) public returns (bool success);
	function allowance(address _owner, address _spender) constant public returns (uint256 remaining);
}

contract StandardToken is Token {
	uint256 supply;
	mapping (address => uint256) balance;
	mapping (address =>
		mapping (address => uint256)) m_allowance;

	function StandardToken(address _initialOwner, uint256 _supply) public {
		supply = _supply;
		balance[_initialOwner] = _supply;
	}

	function balanceOf(address _account) constant public returns (uint256) {
		return balance[_account];
	}

	function totalSupply() constant public returns (uint256) {
		return supply;
	}

	function transfer(address _to, uint256 _value) public returns (bool success) {
		return doTransfer(msg.sender, _to, _value);
	}

	function transferFrom(address _from, address _to, uint256 _value) public returns (bool) {
		if (m_allowance[_from][msg.sender] >= _value) {
			if (doTransfer(_from, _to, _value)) {
				m_allowance[_from][msg.sender] -= _value;
			}
			return true;
		} else {
			return false;
		}
	}

	function doTransfer(address _from, address _to, uint256 _value) internal returns (bool success) {
		if (balance[_from] >= _value && balance[_to] + _value >= balance[_to]) {
			balance[_from] -= _value;
			balance[_to] += _value;
			emit Transfer(_from, _to, _value);
			return true;
		} else {
			return false;
		}
	}

	function approve(address _spender, uint256 _value) public returns (bool success) {
		m_allowance[msg.sender][_spender] = _value;
		emit Approval(msg.sender, _spender, _value);
		return true;
	}

	function allowance(address _owner, address _spender) constant public returns (uint256) {
		return m_allowance[_owner][_spender];
	}
}
)DELIMITER";

static unique_ptr<bytes> s_compiledERC20;

class ERC20TestFramework: public SolidityExecutionFramework {
protected:
  void deployERC20(const u160 &initialOwner, const u256 &supply) {
    if (!s_compiledERC20)
      s_compiledERC20.reset(new bytes(compileContract(ERC20Code, "StandardToken")));

    sendMessage(encodeArgs(initialOwner, supply), "", *s_compiledERC20, true);
    BOOST_REQUIRE(m_status == 0);
  }

  class ERC20Interface: public ContractInterface {
    public:
      ERC20Interface(SolidityExecutionFramework &framework) : ContractInterface(framework) {}

      u256 totalSupply() {
        return callVoidReturnsUInt256("totalSupply");
      }

      u256 balanceOf(const u160 &owner) {
        return callAddressReturnsUInt256("balanceOf", owner);
      }

      bool transfer(const u160 &to, const u256 &value) {
        return callAddressUInt256ReturnsBool("transfer", to, value);
      }

      bool transferFrom(const u160 &spender, const u160 &to, const u256 &value) {
        return callAddressAddressUInt256ReturnsBool("transferFrom", spender, to, value);
      }

      bool approve(const u160 &spender, const u256 &value) {
        return callAddressUInt256ReturnsBool("approve", spender, value);
      }

      u256 allowance(const u160 &owner, const u160 &spender) {
        return callAddressAddressReturnsUInt256("allowance", owner, spender);
      }
  };
};

} // end anonymous namespace

BOOST_FIXTURE_TEST_SUITE(SolidityERC20, ERC20TestFramework)

BOOST_AUTO_TEST_CASE(creation) {
  const u256 &supply = 0x2710;
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.totalSupply(), supply);
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespace test
} // end namespace solidity
} // end namespace dev
