contract C {
    struct S { bool f; }
    S s;
    function f() internal pure returns (S storage c) {
        assembly {
            for { c.slot := s.slot } iszero(0) {} {}
        }
    }
    function g() internal pure returns (S storage c) {
        assembly {
            for { c.slot := s.slot } iszero(1) {} {}
        }
    }
}
// ----
// SyntaxError 1184: (110-183): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (253-326): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
