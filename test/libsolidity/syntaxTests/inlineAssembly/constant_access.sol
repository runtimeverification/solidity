contract C {
    uint constant x = 2**20;
    bool constant b = true;
    bytes4 constant s = "ab";
    function f() public pure {
        assembly {
            let c1 := x
            let c2 := b
            let c3 := s
        }
    }
}
// ----
// SyntaxError 1184: (139-231): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
