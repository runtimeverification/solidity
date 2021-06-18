contract Test {
    function f() public pure returns (uint) {
        return type(C).runtimeCode.length +
            type(D).runtimeCode.length +
            type(C).creationCode.length +
            type(D).creationCode.length;
    }
}
contract C {
    constructor() { assembly {} }
}
contract D is C {
    constructor() {}
}
// ----
// SyntaxError 1184: (271-282): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
