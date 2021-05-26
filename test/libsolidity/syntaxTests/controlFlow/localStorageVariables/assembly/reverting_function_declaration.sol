contract C {
    struct S { bool f; }
    S s;
    function f() internal pure {
        S storage c;
        // this could be allowed, but currently control flow for functions is not analysed
        assembly {
            function f() { revert(0, 0) }
            f()
        }
        c;
    }
}
// ----
// SyntaxError 1184: (200-278): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
