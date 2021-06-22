contract C {
    function h() payable external {
    }
    function f() view external returns (bytes4) {
        function () external g = this.h;
        return g.selector;
    }
}
// ----
// TypeError 2524: (161-171): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
