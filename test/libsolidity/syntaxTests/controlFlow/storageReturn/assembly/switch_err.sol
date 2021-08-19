contract C {
    struct S { bool f; }
    S s;
    function f(uint256 a) internal pure returns (S storage c) {
        assembly {
            switch a
            case 0 { c.slot := s.slot }
        }
    }
    function g(bool flag) internal pure returns (S storage c) {
        assembly {
            switch flag
            case 0 { c.slot := s.slot }
            case 1 { c.slot := s.slot }
        }
    }
    function h(uint256 a) internal pure returns (S storage c) {
        assembly {
            switch a
            case 0 { c.slot := s.slot }
            default { return(0,0) }
        }
    }
}
// ----
// SyntaxError 1184: (119-200): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (279-403): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (482-599): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
