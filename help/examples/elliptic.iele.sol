pragma solidity ^0.4.0;

// 1. inline assembly can be replace by built-in functions.

contract C {
  struct Point {
    uint256 X;
    uint256 Y;
  }

  function addPoints(Point p1, Point p2) internal pure returns (Point retval) {
      uint256[2] memory input1;
      uint256[2] memory input2;
      
      input1[0] = p1.X;
      input1[1] = p1.Y;
      input2[0] = p2.X;
      input2[1] = p2.Y;
      uint256[2] memory output = ecadd(input1, input2);    // <<<- 1
      retval.X = output[0];
      retval.Y = output[1];
  }
}

