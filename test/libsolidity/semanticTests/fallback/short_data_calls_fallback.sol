contract A {
    uint public x;
    // Signature is d88e0b00
    function fow() public { x = 3; }
    fallback () external { x = 2; }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// deposit():
// x() -> 2
// fow(): hex"d88e0b00"
// x() -> 3
// deposit():
// x() -> 2
// fow():
// x() -> 3
// deposit(): hex"d8"
// x() -> 2
