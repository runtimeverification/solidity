contract C {
    function f() public view {
        assembly { pop(chainid()) }
    }
    function g() public view returns (uint) {
        return block.chainid;
    }
}
// ====
// EVMVersion: >=istanbul
// ----
// SyntaxError 1184: (52-79): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
