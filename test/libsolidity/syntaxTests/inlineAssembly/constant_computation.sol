contract C {
    uint constant x = 2**20;
    function f() public pure {
        assembly {
            let a := x
        }
    }
}
// ----
// SyntaxError 1184: (81-124): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
