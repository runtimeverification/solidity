contract C {
    struct S { bool f; }
    S s;
    function f() internal pure {
        S storage c;
        assembly {
            for {} eq(0,0) { c.slot := s.slot } {}
        }
        c;
    }
    function g() internal pure {
        S storage c;
        assembly {
            for {} eq(0,1) { c.slot := s.slot } {}
        }
        c;
    }
    function h() internal pure {
        S storage c;
        assembly {
            for {} eq(0,0) {} { c.slot := s.slot }
        }
        c;
    }
    function i() internal pure {
        S storage c;
        assembly {
            for {} eq(0,1) {} { c.slot := s.slot }
        }
        c;
    }
}
// ----
// SyntaxError 1184: (109-180): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (260-331): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (411-482): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (562-633): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
