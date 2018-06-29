pragma solidity ^0.4.0;

// 1. `address.call` is replaced by an ordinary call
// 2. `msg.data` is replaced by a function argument

contract ByteStoreContract {
  bytes data;

  function set(bytes b) public returns (bool) {      // <<<- 2
    data = b;                                        // <<<- 2
    return true;
  }
  
  function getLength() public view returns (uint) {
    return data.length;
  }
}

// Note also the change from `1` to `uint256(1)`. Constants in
// EVM-Solidity are `uint256`. In IELE-Solidity, they're `uint` (a
// variable-sized type). The cast preserves the meaning of the
// program.

contract ClientContract {
  function test() public returns (uint) {
    bytes4 callee = bytes4(keccak256("set()"));
    
    ByteStoreContract store = new ByteStoreContract();
    store.set(abi.encode(callee, uint256(1), uint256(2), uint256(3)));   // <<<- 1
    return store.getLength();
  }
}

