contract C {
    function f() public pure {
        uint a;
        assembly {
            let a := 1
        }
    }
}
// ----
// SyntaxError 1184: (68-111): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
