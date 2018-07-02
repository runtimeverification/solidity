pragma solidity ^0.4.0;

// 1. `delegatecall` is not allowed in IELE. For simplicity, the
//    library contract is included in this file, rather than
//    found and resolved at link time.

contract LibraryContract {
  event libraryEvent(address from);
  
  function libraryFunction() public {
    emit libraryEvent(this);
  }
}

contract Contract {
  address lib;

  constructor() public {
    lib = new LibraryContract();
  }

  function contractFunction() public {
    lib.delegatecall(bytes4(keccak256("libraryFunction()")));  // <<<- 1
  }
}

contract Driver {
  function test() public {
    Contract c = new Contract();
    c.contractFunction();
  }
}

