pragma solidity ^0.4.0;

// 1. IELE-Solidity contracts can inherit from library contracts.
// 2. Because library code is copied, what used to be
//    `delegatecall` is now an ordinary internal call.

// This would normally be provided via `import`.
contract LibraryContract {
  event libraryEvent(address from);
  
  function libraryFunction() public {
    emit libraryEvent(this);
  }
}

contract Contract is LibraryContract {    // <<<- 1
  function contractFunction() public {
    libraryFunction();                    // <<<- 2
  }
}

contract Driver {
  function test() public {
    Contract c = new Contract();
    c.contractFunction();
  }
}
