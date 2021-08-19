contract C {
    struct S { bool f; }
    S s;
    function f(uint256 a) internal pure returns (S storage c) {
        assembly {
            switch a
                default { c.slot := s.slot }
        }
    }
}
// ----
// SyntaxError 1184: (119-205): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
