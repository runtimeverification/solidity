// SPDX-License-Identifier: UNLICENSED
pragma solidity >= 0.0;

contract StandardToken {
	uint256 supply;
	mapping (address => uint256) balance;
	mapping (address =>
		mapping (address => uint256)) m_allowance;

	constructor(address _initialOwner, uint256 _supply) {
		supply = _supply;
		balance[_initialOwner] = _supply;
	}

	function balanceOf(address _account) public view returns (uint256) {
		return balance[_account];
	}

	function totalSupply() public view returns (uint256) {
		return supply;
	}

	function allowance(address _owner, address _spender) public view returns (uint256) {
		return m_allowance[_owner][_spender];
	}

	function transfer(address _to, uint256 _value) public returns (bool success) {
		return doTransfer(msg.sender, _to, _value);
	}

	function approve(address _spender, uint256 _value) public returns (bool success) {
		m_allowance[msg.sender][_spender] = _value;
		return true;
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
			return true;
		} else {
			return false;
		}
	}
}

contract Actor {
  StandardToken token;

  function setToken(StandardToken _token) public {
    token = _token;
  }

  function transfer(Actor to, uint256 value) public returns (bool) {
    return token.transfer(address(to), value);
  }

  function approve(Actor spender, uint256 value) public returns (bool) {
    return token.approve(address(spender), value);
  }

  function transferFrom(Actor from, Actor to, uint256 value) public returns (bool) {
    return token.transferFrom(address(from), address(to), value);
  }
}

contract Driver {
    function simpleTransfer(uint256 supply,
                            uint256 transferAmount)
      public
      returns (bool success,
               uint256 ownerBalance,
               uint256 targetBalance) {

        Actor owner = new Actor();
        StandardToken token = new StandardToken(address(owner), supply);
        owner.setToken(token);
 
        Actor target = new Actor();
        target.setToken(token);

        success = owner.transfer(target, transferAmount);
        ownerBalance = token.balanceOf(address(owner));
        targetBalance = token.balanceOf(address(target));
    }

    function allowanceTransfer(uint256 supply,
                               uint256 allowanceAmount,
                               uint256 transferAmount)
      public
      returns (bool success,
               uint256 ownerBalance,
               uint256 targetBalance,
               uint256 spenderAllowance) {

        Actor owner = new Actor();
        StandardToken token = new StandardToken(address(owner), supply);
        owner.setToken(token);
 
        Actor spender = new Actor();
        spender.setToken(token);

        Actor target = new Actor();
        target.setToken(token);

        owner.approve(spender, allowanceAmount);
        success = spender.transferFrom(owner, target, transferAmount);
        ownerBalance = token.balanceOf(address(owner));
        targetBalance = token.balanceOf(address(target));
        spenderAllowance = token.allowance(address(owner), address(spender));
    }
}

// ====
// ----
// simpleTransfer(uint256,uint256) : 1000, 400 -> true, 600, 400
// simpleTransfer(uint256,uint256) : 1000, 1000 -> true, 0, 1000
// simpleTransfer(uint256,uint256) : 1000, 2000 -> false, 1000, 0
// allowanceTransfer(uint256,uint256,uint256) : 1000, 400, 300 -> true, 700, 300, 100
// allowanceTransfer(uint256,uint256,uint256) : 1000, 400, 400 -> true, 600, 400, 0
// allowanceTransfer(uint256,uint256,uint256) : 1000, 300, 400 -> false, 1000, 0, 300
