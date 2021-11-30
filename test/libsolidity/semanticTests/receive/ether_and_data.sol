contract C {
    receive () payable external { }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// (), 1 wei
// (), 1 wei: 1 -> FAILURE, 2
