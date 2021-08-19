contract C {
    function f(uint[] calldata bytesAsCalldata) external {
        assembly {
            let x := bytesAsCalldata
        }
    }
}
// ----
// SyntaxError 1184: (80-137): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
