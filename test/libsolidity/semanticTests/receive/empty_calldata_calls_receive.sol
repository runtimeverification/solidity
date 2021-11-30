contract A {
    uint public x;
    receive () external payable { ++x; }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// x() -> 0
// ()
// x() -> 1
// (), 1 wei
// x() -> 2
// x(), 1 wei -> FAILURE, 255
// (): hex"00" -> FAILURE, 2
// (), 1 wei: hex"00" -> FAILURE, 2
