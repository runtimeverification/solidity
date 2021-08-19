contract C {
    struct S { bool f; }
    S s;
    function f(uint256 a) internal pure {
        S storage c;
        assembly {
            switch a
            case 0 { c.slot := s.slot }
        }
        c;
    }
    function g(bool flag) internal pure {
        S storage c;
        assembly {
            switch flag
            case 0 { c.slot := s.slot }
            case 1 { c.slot := s.slot }
        }
        c;
    }
    function h(uint256 a) internal pure {
        S storage c;
        assembly {
            switch a
            case 0 { c.slot := s.slot }
            default { return(0,0) }
        }
        c;
    }
}
// ----
// SyntaxError 1184: (118-199): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (288-412): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (501-618): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
