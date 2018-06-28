pragma solidity ^0.4.0;

contract LibraryContract {
  event libraryEvent(address from);
  
  function libraryFunction() public {
    emit libraryEvent(this);
  }
}

contract Contract is LibraryContract {
  function contractFunction() public {
    libraryFunction();
  }
}

contract Driver {
  function test() public {
    Contract c = new Contract();
    c.contractFunction();
  }
}
