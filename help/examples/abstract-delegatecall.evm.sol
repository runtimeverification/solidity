pragma solidity ^0.4.0;

// IELE-Solidity requires source before it will compile a call to 
// a library contract. So the use of the abstract contract below
// cannot be converted to IELE.

contract LibraryContract {
  function libraryFunction() public;
}

contract Contract {
  address lib;

  constructor() public {
    lib = LibraryContract(0x3bBb367Afe5075E0461F535d6Ed2A640822EDb1C);
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

