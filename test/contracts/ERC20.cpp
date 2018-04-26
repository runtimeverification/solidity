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
pragma solidity ^0.4.22;

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

	constructor(address _initialOwner, uint256 _supply) public {
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

  void checkERC20Log(const std::string &EventName, const u160 &address1,
                            const u160 &address2, const u256 &value) const {
    BOOST_REQUIRE_EQUAL(m_logs.size(), 1);
    BOOST_CHECK_EQUAL(m_logs[0].address, m_contractAddress);
    BOOST_CHECK_EQUAL(h256(m_logs[0].data, h256::AlignLeft),
                      h256(encodeLogs(h256(value))));
    BOOST_REQUIRE_EQUAL(m_logs[0].topics.size(), 3);
    BOOST_CHECK_EQUAL(m_logs[0].topics[0],
                      dev::keccak256(EventName + "(address,address,uint256)"));
    BOOST_CHECK_EQUAL(m_logs[0].topics[1], h256(h160(address1), h256::AlignRight));
    BOOST_CHECK_EQUAL(m_logs[0].topics[2], h256(h160(address2), h256::AlignRight));
  }

  void checkERC20TransferLog(
      const u160 &from, const u160 &to, const u256 &value) const {
    checkERC20Log("Transfer", from, to, value);
  }

  void checkERC20ApprovalLog(
      const u160 &owner, const u160 &spender, const u256 &value) const {
    checkERC20Log("Approval", owner, spender, value);
  }

  void checkERC20EmptyLog() const { BOOST_CHECK_EQUAL(m_logs.size(), 0); }

  u160 makeAccountWithEther(int id) {
    u160 accnt = account(id);
    sendEther(accnt, 10 * ether);
    return accnt;
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

BOOST_AUTO_TEST_CASE(allowance_CallerCaller) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), u256(0));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(allowance_CallerOther) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(0x4);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, other), u256(0));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(allowance_OtherCaller) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(0x8);
  BOOST_CHECK_EQUAL(erc20.allowance(other, m_sender), u256(0));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(allowance_OtherEqOther) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(0x2);
  BOOST_CHECK_EQUAL(erc20.allowance(other, other), u256(0));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(allowance_OtherNEqOther) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other1 = u160(0x9);
  const u160 &other2 = u160(0x5);
  BOOST_CHECK_EQUAL(erc20.allowance(other1, other2), u256(0));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(approve_Caller_Negative) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  ABI_CHECK(callContractFunction("approve(address,uint256)", m_sender, -3),
            std::vector<bytes>());
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), u256(0));
}

BOOST_AUTO_TEST_CASE(approve_Caller_Positive) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u256 &value = u256(0x25);
  BOOST_CHECK_EQUAL(erc20.approve(m_sender, value), true);
  checkERC20ApprovalLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), value);
}

BOOST_AUTO_TEST_CASE(approve_Caller_Zero) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u256 &value = u256(0);
  BOOST_CHECK_EQUAL(erc20.approve(m_sender, value), true);
  checkERC20ApprovalLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), value);
}

BOOST_AUTO_TEST_CASE(approve_Other_Negative) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(3);
  ABI_CHECK(callContractFunction("approve(address,uint256)", other, -3),
            std::vector<bytes>());
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, other), u256(0));
}

BOOST_AUTO_TEST_CASE(approve_Other_Positive) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(8);
  const u256 &value = u256(0x2a);
  BOOST_CHECK_EQUAL(erc20.approve(other, value), true);
  checkERC20ApprovalLog(m_sender, other, value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, other), value);
}

BOOST_AUTO_TEST_CASE(approve_Other_Zero) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(0x8);
  const u256 &value = u256(0);
  BOOST_CHECK_EQUAL(erc20.approve(other, value), true);
  checkERC20ApprovalLog(m_sender, other, value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, other), value);
}

BOOST_AUTO_TEST_CASE(approve_SwitchCaller) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = makeAccountWithEther(2);
  const u256 &value = u256(0x19);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), u256(0));
  checkERC20EmptyLog();
  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, value), true);
  checkERC20ApprovalLog(owner, spender, value);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), u256(0x19));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(balanceOf_Caller) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(balanceOf_NonCaller) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  const u160 &other = u160(0x6);
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), u256(0));
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(create) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.totalSupply(), supply);
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(totalSupply_Positive) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.totalSupply(), supply);
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(totalSupply_Zero) {
  const u256 &supply = u256(0);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.totalSupply(), supply);
  checkERC20EmptyLog();
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_BalanceEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x17);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20TransferLog(owner, target, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply - value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), value);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance - value);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_BalanceNEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0x17);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20TransferLog(owner, target, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply - value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), value);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance - value);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_EntireAllowanceMoreThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2711);
  const u256 &value = u256(0x2711);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_EntireBalanceEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x2710);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20TransferLog(owner, target, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), u256(0));
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), value);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), u256(0));
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_EntireBalanceMoreThanAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x270f);
  const u256 &value = u256(0x2710);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_MoreThanAllowanceLessThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0x29);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_MoreThanBalanceLessThanAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x3000);
  const u256 &value = u256(0x2711);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllDistinct_Negative) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = u160(0x5);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  ABI_CHECK(callContractFunction("transferFrom(address,address,uint256)",
                                 owner, target, -3),
            std::vector<bytes>());
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllEqual_AllowanceRelevant) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x14);
  const u256 &value = u256(0x17);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transferFrom(m_sender, m_sender, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_AllEqual_EntireBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transferFrom(m_sender, m_sender, value), true);
  checkERC20TransferLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), u256(0));
}

BOOST_AUTO_TEST_CASE(transferFrom_CallerEqFrom_AllowanceRelevant) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x14);
  const u256 &value = u256(0x17);
  const u160 &target = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transferFrom(m_sender, target, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_CallerEqFrom_EntireBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x2710);
  const u160 &target = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transferFrom(m_sender, target, value), true);
  checkERC20TransferLog(m_sender, target, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), u256(0));
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), u256(0));
}

BOOST_AUTO_TEST_CASE(transferFrom_CallerEqFrom_MoreThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x2711);
  const u160 &target = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transferFrom(m_sender, target, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_CallerEqTo_BalanceNEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0x17);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(target, allowance), true);
  checkERC20ApprovalLog(owner, target, allowance);
  m_sender = target;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20TransferLog(owner, target, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply - value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), value);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, target), allowance - value);
}

BOOST_AUTO_TEST_CASE(transferFrom_CallerEqTo_MoreThanAllowanceLessThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0x29);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(target, allowance), true);
  checkERC20ApprovalLog(owner, target, allowance);
  m_sender = target;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, target), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_CallerEqTo_MoreThanBalanceLessThanAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x30000);
  const u256 &value = u256(0x27100);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(target, allowance), true);
  checkERC20ApprovalLog(owner, target, allowance);
  m_sender = target;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target, value), true);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, target), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_Exploratory_MultipleTransfersSucceed) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0xa);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target1 = u160(0x5);
  const u160 &target2 = u160(0x6);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target1, value), true);
  checkERC20TransferLog(owner, target1, value);
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target2, value), true);
  checkERC20TransferLog(owner, target2, value);
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20TransferLog(owner, owner, value);
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, spender, value), true);
  checkERC20TransferLog(owner, spender, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply - (value * 3));
  BOOST_CHECK_EQUAL(erc20.balanceOf(target1), value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target2), value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(spender), value);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), u256(0));
}

BOOST_AUTO_TEST_CASE(transferFrom_Exploratory_MultipleTransfersThrow) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0xa);
  const u256 &valueToSpender = u256(0xb);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &target1 = u160(0x5);
  const u160 &target2 = u160(0x6);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target1, value), true);
  checkERC20TransferLog(owner, target1, value);
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, target2, value), true);
  checkERC20TransferLog(owner, target2, value);
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20TransferLog(owner, owner, value);
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, spender, valueToSpender), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply - (value * 2));
  BOOST_CHECK_EQUAL(erc20.balanceOf(target1), value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(target2), value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(spender), u256(0));
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance - (value *3));
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_BalanceEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x17);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20TransferLog(owner, owner, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance - value);
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_BalanceNEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0x17);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20TransferLog(owner, owner, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance - value);
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_EntireAllowanceMoreThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2711);
  const u256 &value = u256(0x2711);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_EntireBalanceEqAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x2710);
  const u256 &value = u256(0x2710);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20TransferLog(owner, owner, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), u256(0));
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_EntireBalanceMoreThanAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x270f);
  const u256 &value = u256(0x2710);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_MoreThanAllowanceLessThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u256 &value = u256(0x29);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_MoreThanBalanceLessThanAllowance) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x3000);
  const u256 &value = u256(0x2711);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  BOOST_CHECK_EQUAL(erc20.transferFrom(owner, owner, value), true);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transferFrom_FromEqTo_Negative) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x28);
  const u160 &owner = makeAccountWithEther(1);
  const u160 &spender = m_sender;
  deployERC20(owner, supply);
  ERC20Interface erc20(*this);

  m_sender = owner;
  BOOST_CHECK_EQUAL(erc20.approve(spender, allowance), true);
  checkERC20ApprovalLog(owner, spender, allowance);
  m_sender = spender;
  ABI_CHECK(callContractFunction("transferFrom(address,address,uint256)",
                                 owner, owner, -3),
            std::vector<bytes>());
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(owner), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(owner, spender), allowance);
}

BOOST_AUTO_TEST_CASE(transfer_Caller_AllowanceIrrelevant) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x14);
  const u256 &value = u256(0x17);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transfer(m_sender, value), true);
  checkERC20TransferLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), allowance);
}

BOOST_AUTO_TEST_CASE(transfer_Caller_EntireBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(m_sender, value), true);
  checkERC20TransferLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
}

BOOST_AUTO_TEST_CASE(transfer_Caller_MoreThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0x2711);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(m_sender, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
}

BOOST_AUTO_TEST_CASE(transfer_Caller_Negative) {
  const u256 &supply = u256(0x2710);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  ABI_CHECK(callContractFunction("transfer(address,uint256)", m_sender, -1),
            std::vector<bytes>());
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
}

BOOST_AUTO_TEST_CASE(transfer_Caller_Positive) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0x17);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(m_sender, value), true);
  checkERC20TransferLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
}

BOOST_AUTO_TEST_CASE(transfer_Caller_Zero) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(m_sender, value), true);
  checkERC20TransferLog(m_sender, m_sender, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
}

BOOST_AUTO_TEST_CASE(transfer_Other_AllowanceIrrelevant) {
  const u256 &supply = u256(0x2710);
  const u256 &allowance = u256(0x14);
  const u256 &value = u256(0x17);
  const u160 &other = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.approve(m_sender, allowance), true);
  checkERC20ApprovalLog(m_sender, m_sender, allowance);
  BOOST_CHECK_EQUAL(erc20.transfer(other, value), true);
  checkERC20TransferLog(m_sender, other, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply - value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), value);
  BOOST_CHECK_EQUAL(erc20.allowance(m_sender, m_sender), allowance);
}

BOOST_AUTO_TEST_CASE(transfer_Other_EntireBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0x2710);
  const u160 &other = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(other, value), true);
  checkERC20TransferLog(m_sender, other, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), u256(0));
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), value);
}

BOOST_AUTO_TEST_CASE(transfer_Other_MoreThanBalance) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0x2711);
  const u160 &other = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(other, value), false);
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), u256(0));
}

BOOST_AUTO_TEST_CASE(transfer_Other_Negative) {
  const u256 &supply = u256(0x2710);
  const u160 &other = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  ABI_CHECK(callContractFunction("transfer(address,uint256)", other, -1),
            std::vector<bytes>());
  checkERC20EmptyLog();
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), u256(0));
}

BOOST_AUTO_TEST_CASE(transfer_Other_Positive) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0x17);
  const u160 &other = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(other, value), true);
  checkERC20TransferLog(m_sender, other, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply - value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), value);
}

BOOST_AUTO_TEST_CASE(transfer_Other_Zero) {
  const u256 &supply = u256(0x2710);
  const u256 &value = u256(0);
  const u160 &other = u160(0x4);
  deployERC20(m_sender, supply);
  ERC20Interface erc20(*this);

  BOOST_CHECK_EQUAL(erc20.transfer(other, value), true);
  checkERC20TransferLog(m_sender, other, value);
  BOOST_CHECK_EQUAL(erc20.balanceOf(m_sender), supply);
  BOOST_CHECK_EQUAL(erc20.balanceOf(other), value);
}

BOOST_AUTO_TEST_SUITE_END()

} // end namespace test
} // end namespace solidity
} // end namespace dev
