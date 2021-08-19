library L {
    function f(uint256) external pure {}
    function g(uint256) external view {}
}
contract C {
    function f() public pure returns (bytes4, bytes4) {
        return (L.f.selector, L.g.selector);
    }
}
// ----
// TypeError 2524: (181-193): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (195-207): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
