pragma solidity ^0.4.0;

// 1. IELE-Solidity has a codesize member on address

contract C {
  function isAddressContract(address addr) view internal returns (bool) {
    uint16 size = addr.codesize;
    return size > 0;
  }
}
