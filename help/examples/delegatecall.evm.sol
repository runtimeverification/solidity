pragma solidity ^0.4.0;

// 1. `delegatecall` is not allowed in IELE.

contract LibraryContract {
  event libraryEvent(address from);
  
  function libraryFunction() public {
    emit libraryEvent(this);
  }
}

contract Contract {
  address lib;

  constructor() public {
    lib = new LibraryContract();       // <<<- 1
  }

  function contractFunction() public {
    lib.delegatecall(bytes4(keccak256("libraryFunction()")));  // <<<- 2
  }
}

contract Driver {
  function test() public {
    Contract c = new Contract();
    c.contractFunction();
  }
}

