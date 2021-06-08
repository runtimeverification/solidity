contract C {
    function f() public view {
        assembly { pop(extcodehash(0)) }
    }
}
// ====
// EVMVersion: >=constantinople
// ----
// SyntaxError 1184: (52-84): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
