contract C {
    struct S { bool f; }
    S s;
    function f(bool flag) internal pure {
        S storage c;
        assembly {
            if flag { c.slot := s.slot }
        }
        c;
    }
}
// ----
// SyntaxError 1184: (118-179): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
