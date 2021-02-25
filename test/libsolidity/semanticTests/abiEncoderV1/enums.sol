contract C {
    enum E { A, B }
    function f(E e) public pure returns (uint x) {
        x = uint(e);
    }
}
// ====
// ABIEncoderV1Only: true
// ----
// f(uint8): 0 -> 0
// f(uint8): 1 -> 1
// f(uint8): 2 -> 2
// f(uint8): 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff -> 0xff
