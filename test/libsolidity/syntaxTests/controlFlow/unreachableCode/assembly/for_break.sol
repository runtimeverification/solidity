contract C {
    function f() public pure {
        assembly {
            for { let a := 0} lt(a,1) { a := add(a, 1) } {
                break
                let b := 42
            }
        }
    }
}
// ----
// SyntaxError 1184: (52-195): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
