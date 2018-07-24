pragma solidity ^0.4.0;

// 1. inline assembly is not allowed in IELE.

contract C {
  struct Point {
    uint256 X;
    uint256 Y;
  }

  function addPoints(Point p1, Point p2) internal returns (Point retval) {
     uint256[4] memory temp;
     temp[0] = p1.X;
     temp[1] = p1.Y;
     temp[2] = p2.X;
     temp[3] = p2.Y;
     bool success;
     assembly {                                    // <<<- 1
         let ecadd := 6 // precompiled contract
         success := call(sub(gas, 2000), ecadd, 0, temp, 0xc0, retval, 0x60)
         // Use "invalid" to make gas estimation work
         switch success case 0 { invalid() }
     }
     require(success);
  }

}
