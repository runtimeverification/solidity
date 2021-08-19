contract C {
    function f() public pure {}
    constructor() {
        assembly {
            let x := f
        }
    }
}
// ----
// SyntaxError 1184: (73-116): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
