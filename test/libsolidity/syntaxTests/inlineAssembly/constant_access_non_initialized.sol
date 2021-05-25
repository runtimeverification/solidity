contract C {
    uint constant x;
    function f() public pure {
        assembly {
            let c1 := x
        }
    }
}
// ----
//  SyntaxError 1184: (73-117): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
//  TypeError 4266: (17-32): Uninitialized "constant" variable.
