contract C {
    function f() view external returns (uint id) {
        assembly {
            id := chainid()
        }
    }
    function g() view external returns (uint sb) {
        assembly {
            sb := selfbalance()
        }
    }
}
// ====
// EVMVersion: >=istanbul
// ----
// SyntaxError 1184: (72-120): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (186-238): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
