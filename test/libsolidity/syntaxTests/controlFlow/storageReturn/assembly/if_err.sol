contract C {
    struct S { bool f; }
    S s;
    function f(bool flag) internal pure returns (S storage c) {
        assembly {
            if flag { c.slot := s.slot }
        }
    }
}
// ----
// SyntaxError 1184: (119-180): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
