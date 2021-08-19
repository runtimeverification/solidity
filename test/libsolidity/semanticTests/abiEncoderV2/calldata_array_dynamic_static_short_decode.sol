pragma abicoder               v2;
contract C {
    function f(uint256[][2][] calldata x) external returns (uint256) {
        x[0]; // trigger bounds checks
        return 23;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256[][2][]): refargs { 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 } -> 23 
// f(uint256[][2][]): refargs { 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 } -> 23
// f(uint256[][2][]): refargs { 0x01, 0x01, 0x01, 0x00 } -> 23
