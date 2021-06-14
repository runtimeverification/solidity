contract C {
    function h() pure external {
    }
    function f() pure external returns (bytes4) {
        return this.h.selector;
    }
}
// ----
// TypeError 2524: (117-132): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
