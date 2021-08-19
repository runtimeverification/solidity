pragma abicoder               v2;
contract C {
    function f(bytes[] calldata a, uint256 i) external returns (uint) {
        return uint8(a[0][i]);
    }
}
// ====
// compileViaYul: also
// ----
// f(bytes[],uint256): refargs { 0x01, 0x01, "\x61\x62" }, 0 -> 0x61
// f(bytes[],uint256): refargs { 0x01, 0x01, "\x61\x62" }, 1 -> 0x62
// f(bytes[],uint256): refargs { 0x01, 0x01, "\x61\x62" }, 2 -> FAILURE, 255
