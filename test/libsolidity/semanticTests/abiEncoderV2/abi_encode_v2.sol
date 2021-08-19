pragma abicoder               v2;


contract C {
    struct S {
        uint256 a;
        uint256[] b;
    }

    function f0() public pure returns (bytes memory) {
        return abi.encode();
    }

    function f1() public pure returns (bytes memory) {
        return abi.encode(1, 2);
    }

    function f2() public pure returns (bytes memory) {
        string memory x = "abc";
        return abi.encode(1, x, 2);
    }

    function f3() public pure returns (bytes memory r) {
        // test that memory is properly allocated
        string memory x = "abc";
        r = abi.encode(1, x, 2);
        bytes memory y = "def";
        require(y[0] == "d");
        y[0] = "e";
        require(y[0] == "e");
    }

    S s;

    function f4() public returns (bytes memory r) {
        string memory x = "abc";
        s.a = 7;
        s.b.push(2);
        s.b.push(3);
        r = abi.encode(1, x, s, 2);
        bytes memory y = "def";
        require(y[0] == "d");
        y[0] = "e";
        require(y[0] == "e");
    }
}

// ====
// compileViaYul: also
// ----
// f0() -> ""
// f1() -> "\x01\x02"
// f2() -> "\x01\x03\x00\x00\x00\x00\x00\x00\x00cba\x02"
// f3() -> "\x01\x03\x00\x00\x00\x00\x00\x00\x00cba\x02"
// f4() -> "\x01\x03\x00\x00\x00\x00\x00\x00\x00cba\x07\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x02\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02"
