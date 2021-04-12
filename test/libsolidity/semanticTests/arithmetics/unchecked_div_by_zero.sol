contract C {
    function div(uint256 a, uint256 b) public returns (uint256) {
        // Does not disable div by zero check
        unchecked {
            return a / b;
        }
    }

    function mod(uint256 a, uint256 b) public returns (uint256) {
        // Does not disable div by zero check
        unchecked {
            return a % b;
        }
    }
}
// ====
// compileViaYul: also
// ----
// div(uint256,uint256): 7, 2 -> 3
// div(uint256,uint256): 7, 0 -> FAILURE, 4 # throws #
// mod(uint256,uint256): 7, 2 -> 1
// mod(uint256,uint256): 7, 0 -> FAILURE, 4 # throws #
