contract test {
    uint x = 1;
    modifier m {
        assembly {
            x := 2
        }
        _;
    }
    function f() public m {
    }
}
// ----
// SyntaxError 1184: (57-96): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
