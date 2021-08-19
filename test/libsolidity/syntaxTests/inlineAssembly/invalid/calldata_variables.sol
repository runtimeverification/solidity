contract C {
    function f(bytes calldata bytesAsCalldata) external {
        assembly {
            let x := bytesAsCalldata
        }
    }
}
// ----
// SyntaxError 1184: (79-136): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
