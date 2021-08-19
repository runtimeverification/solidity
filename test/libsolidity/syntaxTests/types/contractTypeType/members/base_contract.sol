contract B {
    function f() external {}
    function g() public {}
}
contract C is B {
    function h() public returns (bytes4 fs, bytes4 gs) {
        fs = B.f.selector;
        gs = B.g.selector;
        B.g();
    }
}
// ----
// TypeError 2524: (159-171): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (186-198): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
