contract C {
    function f() public pure {
        uint mload;
    }
    function g() public pure {
        uint mload;
        assembly {
        }
    }
}
// ----
// SyntaxError 1184: (129-149): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
