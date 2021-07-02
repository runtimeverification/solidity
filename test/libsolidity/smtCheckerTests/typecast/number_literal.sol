pragma experimental SMTChecker;

contract C {
  function f() public pure {
    uint256 x = 1234;
    uint256 y = 0;
    assert(x != y);
    assert(x == uint256(1234));
    assert(y == uint256(0));
  }
  function g() public pure {
    uint256 a = uint256(0);
    uint256 b = type(uint256).max;
    uint256 c = 115792089237316195423570985008687907853269984665640564039457584007913129639935;
    int256 d = -1;
    uint256 e = uint256(d);
    assert(a != b);
    assert(b == c);
    assert(b == e);
  }
  function h() public pure {
    uint32 a = uint32(0);
    uint32 b = type(uint32).max;
    uint32 c = 4294967295;
    int32 d = -1;
    uint32 e = uint32(d);
    assert(a != b);
    assert(b == c);
    assert(b == e);
  }
}
