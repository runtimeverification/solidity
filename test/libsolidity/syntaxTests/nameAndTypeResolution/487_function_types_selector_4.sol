contract C {
    function f() pure external returns (bytes4) {
        return this.f.selector;
    }
}
// ----
// TypeError 2524: (78-93): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
