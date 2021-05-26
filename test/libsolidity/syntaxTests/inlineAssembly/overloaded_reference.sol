contract C {
    function f() pure public {}
    function f(address) pure public {}
    function g() pure public {
        assembly {
            let x := f
        }
    }
}
// ----
// SyntaxError 1184: (123-166): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
