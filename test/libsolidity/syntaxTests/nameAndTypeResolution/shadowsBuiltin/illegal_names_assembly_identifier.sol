contract C {
    function f() public {
        assembly {
            let super := 1
            let this := 1
            let _ := 1
        }
    }
}
// ----
// SyntaxError 1184: (47-143): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
