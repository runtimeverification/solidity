interface A {
    function f() external;
}
contract B {
    function g() public {}
}

contract C is B {
    function h() external {}
    bytes4 constant s1 = A.f.selector;
    bytes4 constant s2 = B.g.selector;
    bytes4 constant s3 = this.h.selector;
    bytes4 constant s4 = super.g.selector;
}
// ----
// TypeError 2524: (158-170): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (197-209): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (236-251): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (278-294): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md

