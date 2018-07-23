pragma solidity ^0.4.0;

// 1. `msg.data` is replaced by a function argument
// 2. `address.call` is replaced by an ordinary call

contract ByteStoreContract {
  bytes data;

  function set(bytes b) public returns (bool) {      // <<<- 1
    data = b;                                        // <<<- 1
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
    // Note: the use of `set()` here is just to make the bytestring
    // byte-for-byte identical with the EVM version. It actually
    // has no effect on which function gets called. (That's determined
    // solely by the `store.set` on the line marked `<<<- 2`.)
    bytes4 callee = bytes4(keccak256("set()"));
    
    ByteStoreContract store = new ByteStoreContract();
    store.set(abi.encode(callee, uint256(1), uint256(2), uint256(3)));   // <<<- 2
    return store.getLength();
  }
}

