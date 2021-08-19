contract test {
    uint a;
    function f() pure public {
        assembly {
            function g() -> x { x := a.slot }
        }
    }
}
// ----
// SyntaxError 1184: (67-133): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
