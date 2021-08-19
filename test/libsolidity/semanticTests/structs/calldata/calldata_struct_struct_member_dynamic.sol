pragma abicoder v2;

contract C {
    struct S {
        uint64 a;
        bytes b;
    }
    struct S1 {
        uint256 a;
        S s;
        uint256 c;
    }

    function f(S1 calldata s1)
        external
        pure
        returns (uint256 a, uint64 b0, bytes1 b1, uint256 c)
    {
        a = s1.a;
        b0 = s1.s.a;
        b1 = s1.s.b[0];
        c = s1.c;
    }
}
// ====
// compileViaYul: also
// ----
// f((uint256,(uint64, bytes),uint256)): refargs { 0x000000000000000000000000000000000000000000000000000000000000002a, 0x0000000000000001, "ab", 0x0000000000000000000000000000000000000000000000000000000000000017 } -> 42, 1, 0x61, 23
