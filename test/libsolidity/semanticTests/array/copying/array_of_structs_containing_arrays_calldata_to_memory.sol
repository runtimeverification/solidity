pragma abicoder v2;

contract C {
    struct S {
        uint[] a;
    }

    function f(S[] memory c) external returns (uint, uint) {
        S[] memory s = c;
        assert(s.length == c.length);
        for (uint i = 0; i < s.length; i++) {
            assert(s[i].a.length == c[i].a.length);
            for (uint j = 0; j < s[i].a.length; j++) {
                assert(s[i].a[j] == c[i].a[j]);
            }
        }
        return (s[1].a.length, s[1].a[0]);
    }
}
// ====
// compileViaYul: also
// ----
// f((uint[])[]): refargs { 0x01, 0x03, 0x01, 0x03, 1, 2, 3, 0x01, 0x03, 1, 2, 3, 0x01, 0x03, 1, 2, 3 } -> 3, 1
