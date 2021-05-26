contract test {
    uint[] r;
    function f() public {
        uint[] storage a = r;
        assembly {
            function g() -> x { x := a.offset }
        }
    }
}
// ----
// SyntaxError 1184: (94-162): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
