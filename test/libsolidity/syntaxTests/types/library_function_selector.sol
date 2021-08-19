library L {
    function f(uint256) external {}
    function g(uint256[] storage) external {}
    function h(uint256[] memory) public {}
}
contract C {
    function f() public pure returns (bytes4 a, bytes4 b, bytes4 c, bytes4 d) {
        a = L.f.selector;
        b = L.g.selector;
        c = L.h.selector;
        d = L.h.selector;
    }
}
// ----
// TypeError 2524: (244-256): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (270-282): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (296-308): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (322-334): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
