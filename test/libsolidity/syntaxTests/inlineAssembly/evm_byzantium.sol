contract C {
    function f() pure external {
        assembly {
            let s := returndatasize()
            returndatacopy(0, 0, s)
        }
    }
    function g() view external returns (uint ret) {
        assembly {
            ret := staticcall(0, gas(), 0, 0, 0, 0)
        }
    }
}
// ====
// EVMVersion: >=byzantium
// ----
// SyntaxError 1184: (54-148): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (215-287): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
