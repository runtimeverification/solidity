contract test {
    function boo() public returns (bytes4 value) {
        return msg.sig;
    }

    function foo(uint a) public returns (bytes4 value) {
        return boo();
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// foo(uint): 0x0 -> 0x2fbebd3800000000000000000000000000000000000000000000000000000000
