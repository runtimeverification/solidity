pragma abicoder               v2;
contract C {
    function f(uint256[][2][] calldata x) external returns (uint256) {
        return 42;
    }
    function g(uint256[][2][] calldata x) external returns (uint256) {
        return this.f(x);
    }
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// g(uint256[][2][]): refargs { 0x01, 0x01, 0x01, 0x00 } -> 42
