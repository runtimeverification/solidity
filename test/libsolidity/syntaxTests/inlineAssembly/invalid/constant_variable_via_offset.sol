contract test {
    uint constant x = 2;
    function f() pure public {
        assembly {
            let r := x.offset
        }
    }
}
// ----
// SyntaxError 1184: (80-130): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
