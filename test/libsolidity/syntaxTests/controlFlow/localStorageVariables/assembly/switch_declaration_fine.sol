contract C {
    struct S { bool f; }
    S s;
    function f(bool flag) internal pure {
        S storage c;
        assembly {
            switch flag
            case 0 { c.slot := s.slot }
            default { c.slot := s.slot }
        }
        c;
    }
    function g(uint256 a) internal pure {
        S storage c;
        assembly {
            switch a
            case 0 { revert(0, 0) }
            default { c.slot := s.slot }
        }
        c;
    }
}
// ----
// SyntaxError 1184: (118-243): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (332-450): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
