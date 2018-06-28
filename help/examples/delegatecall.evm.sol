pragma solidity ^0.4.0;

contract LibraryContract {
  event libraryEvent(address from);
  
  function libraryFunction() public {
    emit libraryEvent(this);
  }
}

contract Contract {
  address lib;

  constructor() {
    lib = new LibraryContract();
  }

  function contractFunction() public {
    lib.delegatecall(bytes4(keccak256("libraryFunction()")));
  }
}

contract Driver {
  function test() public {
    Contract c = new Contract();
    c.contractFunction();
  }
}

