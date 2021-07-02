contract C {
    function f(bytes calldata b, uint i) internal pure returns (bytes1) {
        return b[i];
    }
    function f(uint, bytes calldata b, uint) external pure returns (bytes1) {
        return f(b, 2);
    }
}
// ====
// compileViaYul: also
// ----
// f(uint,bytes,uint): 7, "abcd", 7 -> 0x63
