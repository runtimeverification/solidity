contract C {
    function f() public pure {
        assembly {
            function this() {
            }
            function super() {
            }
            function _() {
            }
        }
    }
}
// ----
// SyntaxError 1184: (52-202): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
