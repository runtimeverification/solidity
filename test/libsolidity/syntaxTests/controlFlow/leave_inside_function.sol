contract C {
    function f() public pure {
        assembly {
            function f() {
                // Make sure this doesn't trigger the unimplemented assertion in the control flow builder.
                leave
            }
        }
    }
}
// ----
// SyntaxError 1184: (52-242): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
