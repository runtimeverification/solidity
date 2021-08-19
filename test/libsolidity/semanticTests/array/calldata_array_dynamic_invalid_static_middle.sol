pragma abicoder               v2;


contract C {
    function f(uint256[][1][] calldata a) external returns (uint256) {
        return 42;
    }

    function g(uint256[][1][] calldata a) external returns (uint256) {
        a[0];
        return 42;
    }

    function h(uint256[][1][] calldata a) external returns (uint256) {
        a[0][0];
        return 42;
    }
}
// ====
// compileViaYul: also
// ----
// f(uint256[][1][]): refargs { 0x01, 0x0 } -> 42 # valid access stub #
// f(uint256[][1][]): refargs { 0x01, 0x01 } -> 42
// f(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x00 } -> 42
// g(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x00 } -> 42
// f(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 } -> 42
// g(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 } -> 42
// h(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x00, 0x01, 0x00 } -> 42
// f(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 } -> 42
// g(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 } -> 42
// h(uint256[][1][]): refargs { 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 } -> 42
