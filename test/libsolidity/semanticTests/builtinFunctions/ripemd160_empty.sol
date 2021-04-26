contract C {
    function f() public returns (bytes20) {
        return ripemd160("");
    }
}

// ====
// compileViaYul: also
// ----
// f() -> 0x009c1185a5c5e9fc54612808977ee8f548b2258d31
