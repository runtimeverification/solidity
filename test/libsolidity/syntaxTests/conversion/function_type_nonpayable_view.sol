contract C {
    function h() external {
    }
    function f() view external returns (bytes4) {
        function () view external g = this.h;
        return g.selector;
    }
}
// ----
// TypeError 9574: (105-141): Type function () external is not implicitly convertible to expected type function () view external.
// TypeError 2524: (158-168): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
