contract C {
    bytes constant a = "\x03\x01\x02";
    bytes constant b = hex"030102";
    string constant c = "hello";

    function f() public returns (bytes memory) {
        return a;
    }

    function g() public returns (bytes memory) {
        return b;
    }

    function h() public returns (bytes memory) {
        return bytes(c);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "\x03\x01\x02"
// g() -> "\x03\x01\x02"
// h() -> "hello"
