library L {
    function double(function(uint256) internal pure returns (uint256) f, uint256 x) internal pure returns (uint256) {
        return f(x) * 2;
    }
}

contract C {
    using L for function(uint256) internal pure returns (uint256);

    function identity(uint256 x) internal pure returns (uint256) {
        return x;
    }

    function test(uint256 value) public returns (uint256) {
        return identity.double(value);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// test(uint256): 5 -> 10
