contract C {
    bytes32 constant x = keccak256("abc");
    bytes32 constant y = x;
    function f() public pure returns (uint t) {
        assembly {
            t := y
        }
    }
}
// ----
// SyntaxError 1184: (140-179): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
