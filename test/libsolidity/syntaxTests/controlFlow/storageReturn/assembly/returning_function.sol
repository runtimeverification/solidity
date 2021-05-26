contract C {
    struct S { bool f; }
    S s;
    function f() internal pure returns (S storage c) {
        // this should warn about unreachable code, but currently function flow is ignored
        assembly {
            function f() { return(0, 0) }
            f()
            c.slot := s.slot
        }
    }
}
// ----
// SyntaxError 1184: (201-308): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
