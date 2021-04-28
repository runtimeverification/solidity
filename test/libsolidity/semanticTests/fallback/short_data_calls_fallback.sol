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
// ()
// x() -> 2
// fow()
// x() -> 3
// ()
// x() -> 2
// fow()
// x() -> 3
// (): hex"d8" -> FAILURE, 2
// x() -> 3
