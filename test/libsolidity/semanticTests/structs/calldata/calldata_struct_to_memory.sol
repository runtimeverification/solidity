pragma abicoder v2;

contract C {
    struct S {
        uint256 a;
        uint256 b;
        bytes2 c;
    }

    function f(S calldata s) external pure returns (uint256, uint256, bytes1) {
        S memory m = s;
        return (m.a, m.b, m.c[1]);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f((uint256,uint256,bytes2)): refargs { 0x000000000000000000000000000000000000000000000000000000000000002a, 0x0000000000000000000000000000000000000000000000000000000000000017, 0x6162 } -> 42, 23, 0x62
