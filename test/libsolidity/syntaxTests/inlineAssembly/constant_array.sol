contract C {
    string constant x = "abc";
    function f() public pure {
        assembly {
            let a := x
        }
    }
}
// ----
// SyntaxError 1184: (83-126): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
