contract test {
    function f() public {
        uint a;
        assembly {
            function g() -> x { x := a }
        }
    }
}
// ----
// SyntaxError 1184: (66-127): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
