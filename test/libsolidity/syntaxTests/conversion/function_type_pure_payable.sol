contract C {
    function h() pure external {
    }
    function f() view external returns (bytes4) {
        function () payable external g = this.h;
        return g.selector;
    }
}
// ----
// TypeError 9574: (110-149): Type function () pure external is not implicitly convertible to expected type function () payable external.
// TypeError 2524: (166-176): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
