contract A {
    function f() public {}
}

contract B {
    function g() external pure returns(bytes4) {
        return A.f.selector;
    }
}
// ----
// TypeError 2524: (120-132): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
