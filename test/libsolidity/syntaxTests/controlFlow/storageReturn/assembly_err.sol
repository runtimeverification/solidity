contract C {
    struct S { bool f; }
    S s;
    function f() internal pure returns (S storage) {
        assembly {
        }
    }
}
// ----
// SyntaxError 1184: (108-128): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
