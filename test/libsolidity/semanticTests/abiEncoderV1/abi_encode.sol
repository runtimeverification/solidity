contract C {
    function f0() public returns (bytes memory) {
        return abi.encode();
    }

    function f1() public returns (bytes memory) {
        return abi.encode(1, 2);
    }

    function f2() public returns (bytes memory) {
        string memory x = "abc";
        return abi.encode(1, x, 2);
    }

    function f3() public returns (bytes memory r) {
        // test that memory is properly allocated
        string memory x = "abc";
        r = abi.encode(1, x, 2);
        bytes memory y = "def";
        require(y[0] == "d");
        y[0] = "e";
        require(y[0] == "e");
    }

    function f4() public returns (bytes memory) {
        bytes4 x = "abcd";
        return abi.encode(bytes2(x));
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f0() -> ""
// f1() -> "\x01\x02"
// f2() -> "\x01\x03\x00\x00\x00\x00\x00\x00\x00cba\x02"
// f3() -> "\x01\x03\x00\x00\x00\x00\x00\x00\x00cba\x02"
// f4() -> "ba"
