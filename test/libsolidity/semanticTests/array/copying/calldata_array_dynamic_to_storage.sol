pragma abicoder               v2;

contract C {
  uint[] s;
  function f(uint[] calldata data) external returns (uint) {
    s = data;
    return s[0];
  }
}
// ====
// compileViaYul: also
// ----
// f(uint[]): dynarray 0 [ 1, 2, 3 ] -> 1
