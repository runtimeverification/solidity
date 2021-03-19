contract C {
    enum X {A, B}

    function test_return() public returns (X) {
        X garbled = X(5);
        return garbled;
    }

    function test_inline_assignment() public returns (X _ret) {
        _ret = X(5);
    }

    function test_assignment() public returns (X _ret) {
        X tmp = X(5);
        _ret = tmp;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// EVMVersion: >=byzantium
// ----
// test_return() -> FAILURE, hex"4e487b71", 33 # both should throw #
// test_inline_assignment() -> FAILURE, hex"4e487b71", 33
// test_assignment() -> FAILURE, hex"4e487b71", 33
