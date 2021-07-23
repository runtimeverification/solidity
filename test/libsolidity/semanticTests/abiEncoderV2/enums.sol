pragma abicoder               v2;

contract C {
    enum E { A, B }
    function f(E e) public pure returns (uint x) {
        x = uint(e);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint8): 0 -> 0
// f(uint8): 1 -> 1
// f(uint8): 2 -> FAILURE, 255
// f(uint8): 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff -> FAILURE, 255
