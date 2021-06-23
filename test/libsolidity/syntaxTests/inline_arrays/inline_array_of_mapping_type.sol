contract C {
  mapping(int => int) a;
  function f() public {
    [a];
  }
}
// ----
// TypeError 1545: (66-69): Type mapping(int => int) is only valid in storage.
