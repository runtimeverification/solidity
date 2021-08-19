contract C {
    function h() pure external {
    }
    function f() view external returns (bytes4) {
        function () view external g = this.h;
        return g.selector;
    }
}
// ----
// TypeError 2524: (163-173): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
