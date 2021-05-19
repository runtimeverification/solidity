interface I {
    function f() external;
}

contract B {
    function g() external pure returns(bytes4) {
        return I.f.selector;
    }
}
// ----
// TypeError 2524: (121-133): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
