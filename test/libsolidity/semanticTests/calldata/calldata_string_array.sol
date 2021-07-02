pragma abicoder               v2;


contract C {
    function f(string[] calldata a)
        external
        returns (uint, uint, uint, string memory)
    {
        string memory s1 = a[0];
        bytes memory m1 = bytes(s1);
        return (a.length, m1.length, uint8(m1[0]), s1);
    }
}
// ====
// compileViaYul: also
// ----
// f(string[]): refargs { 0x01, 0x01, "ab" } -> 1, 2, 97, "ab"
