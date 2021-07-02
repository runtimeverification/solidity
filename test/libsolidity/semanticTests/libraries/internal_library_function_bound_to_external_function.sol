library L {
    // NOTE: External function takes up two stack slots
    function double(function(uint256) external pure returns (uint256) f, uint256 x) internal pure returns (uint256) {
        return f(x) * 2;
    }
}

contract C {
    using L for function(uint256) external pure returns (uint256);

    function identity(uint256 x) external pure returns (uint256) {
        return x;
    }

    function test(uint256 value) public returns (uint256) {
        return this.identity.double(value);
    }
}

// ====
// compileViaYul: also
// ----
// test(uint256): 5 -> 10
