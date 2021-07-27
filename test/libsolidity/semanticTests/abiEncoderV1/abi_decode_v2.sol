pragma abicoder               v2;


contract C {
    struct S {
        uint256 a;
        uint256[] b;
    }

    function f() public pure returns (S memory) {
        S memory s;
        s.a = 8;
        s.b = new uint256[](3);
        s.b[0] = 9;
        s.b[1] = 10;
        s.b[2] = 11;
        return abi.decode(abi.encode(s), (S));
    }
}

// ====
// compileViaYul: also
// ----
// f() -> refargs { 0x0000000000000000000000000000000000000000000000000000000000000008, 0x01, 0x03, 0x0000000000000000000000000000000000000000000000000000000000000009, 0x000000000000000000000000000000000000000000000000000000000000000a, 0x0b }
