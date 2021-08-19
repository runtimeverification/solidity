contract C {
    function f(uint256[2] calldata c) public returns (uint256, uint256) {
        uint256[2] memory m1 = c;
        return (m1[0], m1[1]);
    }
}
// ====
// compileViaYul: also
// ----
// f(uint256[2]): array 256 [ 43, 57 ] -> 43, 57
