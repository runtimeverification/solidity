contract C {
    struct S { bool f; }
    S s;
    function f() internal pure {
        S storage c;
        assembly {
            c.slot := s.slot
        }
        c;
    }
}
// ----
// SyntaxError 1184: (109-158): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
