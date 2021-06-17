pragma abicoder               v2;
contract C {
    bytes[] a;

    function f() public returns (bytes[] memory) {
        a.push("abc");
        a.push("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ");
        bytes[] memory m = a;
        return m;
    }
}
// ----
// f() -> refargs { 0x01, 0x02, "abc", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ" }
