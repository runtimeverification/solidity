contract test {
    modifier m {
        uint a = 1;
        assembly {
            a := 2
        }
        _;
    }
    function f() public m {
    }
}
// ----
// SyntaxError 1184: (61-100): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
