contract C {
    function f(uint256 y) public pure returns (uint256 x) {
        assembly {
            return(0, 0)
            x := y
        }
    }
    function g(uint256 y) public pure returns (uint256 x) {
        assembly {
            return(0, 0)
        }
        x = y;
    }
}
// ----
// SyntaxError 1184: (81-145): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (220-265): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
