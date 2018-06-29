pragma solidity ^0.4.0;

// 1. inline assembly is not allowed in IELE.

contract C {
  function isAddressContract(address addr) view internal returns (bool) {
    uint size;
    assembly { size := extcodesize(addr) }
    return size > 0;
  }
}
