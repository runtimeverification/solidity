contract C {
    function f(uint[] calldata bytesAsCalldata) external pure {
        assembly {
            let x := bytesAsCalldata.offset
        }
    }
}
// ----
// SyntaxError 1184: (85-149): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
