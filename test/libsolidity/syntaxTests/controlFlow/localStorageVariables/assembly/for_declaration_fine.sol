contract C {
    struct S { bool f; }
    S s;
    function f() internal pure {
        S storage c;
        assembly {
            for { c.slot := s.slot } iszero(0) {} {}
        }
        c;
    }
    function g() internal pure {
        S storage c;
        assembly {
            for { c.slot := s.slot } iszero(1) {} {}
        }
        c;
    }
}
// ----
// SyntaxError 1184: (109-182): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (262-335): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
