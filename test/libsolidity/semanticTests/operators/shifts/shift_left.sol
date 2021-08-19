contract C {
    function f(uint256 a, uint256 b) public returns (uint256) {
        return a << b;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint256,uint256): 0x4266, 0x0 -> 0x4266
// f(uint256,uint256): 0x4266, 0x08 -> 0x426600
// f(uint256,uint256): 0x4266, 0x10 -> 0x42660000
// f(uint256,uint256): 0x4266, 0x11 -> 0x0084cc0000
// f(uint256,uint256): 0x4266, 0x00f0 -> 0x4266000000000000000000000000000000000000000000000000000000000000
// f(uint256,uint256): 0x4266, 0x100 -> 0
