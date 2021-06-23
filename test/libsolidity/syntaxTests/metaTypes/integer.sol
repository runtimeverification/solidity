contract Test {
    function basic() public pure {
        uint256 uintMax = type(uint256).max;
        uintMax;
        int256 intMax = type(int256).max;
        intMax;
        uint256 uintMin = type(uint256).min;
        uintMin;
        int256 intMin = type(int256).min;
        intMin;
    }
}
