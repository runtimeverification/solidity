pragma solidity ^0.4.0;

// 1. `msg.data` is not supported
// 2. `address.call` is not supported.

contract ByteStoreContract {
  bytes data;

  function set() public returns (bool) {
    data = msg.data;                                    // <<<- 1
    return true;
  }
  
  function getLength() public view returns (uint) {
    return data.length;
  }
}

contract ClientContract {
  function test() public returns (uint) {
    bytes4 callee = bytes4(keccak256("set()"));
    
    ByteStoreContract store = new ByteStoreContract();
    store.call(callee, 1, 2, 3);                        // <<<- 2
    return store.getLength();
  }
}

