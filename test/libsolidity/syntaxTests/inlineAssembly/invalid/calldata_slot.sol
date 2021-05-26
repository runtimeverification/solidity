contract C {
    function f(bytes calldata bytesAsCalldata) external {
        assembly {
            let x := bytesAsCalldata.slot
        }
    }
}
// ----
// SyntaxError 1184: (79-141): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
