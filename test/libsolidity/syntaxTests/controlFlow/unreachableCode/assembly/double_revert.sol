contract C {
    function f() public pure {
        assembly {
            revert(0, 0)
            revert(0, 0)
        }
    }
    function g() public pure {
        assembly {
            revert(0, 0)
        }
        revert();
    }
}
// ----
// SyntaxError 1184: (52-122): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (168-213): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
