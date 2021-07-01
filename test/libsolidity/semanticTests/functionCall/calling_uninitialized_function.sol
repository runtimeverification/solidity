contract C {
    function intern() public returns (uint256) {
        function (uint) internal returns (uint) x;
        x(2);
        return 7;
    }

    function extern() public returns (uint256) {
        function (uint) external returns (uint) x;
        x(2);
        return 7;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// intern() -> FAILURE, 1 # This should throw exceptions #
// extern() -> FAILURE, 1
