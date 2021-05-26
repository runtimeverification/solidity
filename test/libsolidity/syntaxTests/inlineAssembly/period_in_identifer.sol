contract C {
    function f() pure public {
        // Periods are part of identifiers in assembly,
        // but not in Solidity. This tests that this scanner
        // setting is properly reset early enough.
        assembly { }
        C.f();
    }
}
// ----
// SyntaxError 1184: (220-232): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
