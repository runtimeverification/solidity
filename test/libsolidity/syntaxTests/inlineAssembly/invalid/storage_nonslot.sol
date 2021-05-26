contract test {
    uint x = 1;
    function f() public {
        assembly {
            let t := x.length
            x.length := 2
        }
    }
}
// ----
// SyntaxError 1184: (66-142): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
