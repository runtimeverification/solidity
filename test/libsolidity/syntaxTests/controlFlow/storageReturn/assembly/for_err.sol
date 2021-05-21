contract C {
    struct S { bool f; }
    S s;
    function f() internal pure returns (S storage c) {
        assembly {
            for {} eq(0,0) { c.slot := s.slot } {}
        }
    }
    function g() internal pure returns (S storage c) {
        assembly {
            for {} eq(0,1) { c.slot := s.slot } {}
        }
    }
    function h() internal pure returns (S storage c) {
        assembly {
            for {} eq(0,0) {} { c.slot := s.slot }
        }
    }
    function i() internal pure returns (S storage c) {
        assembly {
            for {} eq(0,1) {} { c.slot := s.slot }
        }
    }
}
// ----
// SyntaxError 1184: (110-181): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (251-322): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (392-463): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (533-604): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
