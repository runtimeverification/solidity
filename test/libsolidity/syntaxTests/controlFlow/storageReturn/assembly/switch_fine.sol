contract C {
    struct S { bool f; }
    S s;
    function f(bool flag) internal pure returns (S storage c) {
        assembly {
            switch flag
            case 0 { c.slot := s.slot }
            default { c.slot := s.slot }
        }
    }
    function g(uint256 a) internal pure returns (S storage c) {
        assembly {
            switch a
            case 0 { revert(0, 0) }
            default { c.slot := s.slot }
        }
    }
}
// ----
// SyntaxError 1184: (119-244): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (323-441): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
