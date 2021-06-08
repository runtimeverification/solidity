contract C {
    struct S { uint x; }
    S s;
    function e() pure public {
        assembly { mstore(keccak256(0, 20), mul(s.slot, 2)) }
    }
    function f() pure public {
        uint x;
        assembly { x := 7 }
    }
    function g() view public {
        assembly { for {} 1 { pop(sload(0)) } { } pop(gas()) }
    }
    function h() view public {
        assembly { function g() { pop(blockhash(20)) } }
    }
    function i() public {
        assembly { pop(call(0, 1, 2, 3, 4, 5, 6)) }
    }
    function j() public {
        assembly { pop(call(gas(), 1, 2, 3, 4, 5, 6)) }
    }
    function k() public view {
        assembly { pop(balance(0)) }
    }
    function l() public view {
        assembly { pop(extcodesize(0)) }
    }
}
// ----
// SyntaxError 1184: (86-139): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (201-220): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (266-320): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (366-414): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (455-498): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (539-586): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (632-660): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (706-738): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
