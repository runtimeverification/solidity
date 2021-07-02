struct Struct { uint256 x; }

library L {
    function f(Struct storage _x) internal view returns (uint256) {
      return _x.x;
    }
}

contract C {
  using L for Struct;

  Struct s;

  function h(Struct storage _s) internal view returns (uint256) {
    // _s is pointer
    return _s.f();
  }
  function g() public returns (uint256, uint256) {
    s.x = 7;
    // s is reference
    return (s.f(), h(s));
  }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// g() -> 7, 7
