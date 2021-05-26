contract C {
    function f() view external returns (uint ret) {
        assembly {
            ret := shl(gas(), 5)
            ret := shr(ret, 2)
            ret := sar(ret, 2)
        }
    }
    function g() external returns (address ret) {
        assembly {
            ret := create2(0, 0, 0, 0)
        }
    }
    function h() view external returns (bytes32 ret) {
        assembly {
            ret := extcodehash(address())
        }
    }
}
// ====
// EVMVersion: >=constantinople
// ----
// SyntaxError 1184: (73-188): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (253-312): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// SyntaxError 1184: (382-444): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
