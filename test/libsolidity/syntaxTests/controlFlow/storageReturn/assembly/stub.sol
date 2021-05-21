contract C {
    struct S { bool f; }
    S s;
    function f() internal pure returns (S storage c) {
        assembly {
            c.slot := s.slot
        }
    }
}
// ----
// SyntaxError 1184: (110-159): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
