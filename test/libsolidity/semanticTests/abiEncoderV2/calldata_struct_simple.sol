pragma abicoder               v2;

contract C {
    struct S { uint256 a; }

    function f(S calldata s) external returns (bytes memory) {
        return abi.encode(s);
    }

    function g(S calldata s) external returns (bytes memory) {
        return this.f(s);
    }
}
// ====
// compileViaYul: also
// EVMVersion: >homestead
// ----
// f((uint256)): refargs { 0x000000000000000000000000000000000000000000000000000000000000003 } -> "\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
// g((uint256)): refargs { 0x000000000000000000000000000000000000000000000000000000000000003 } -> "\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
