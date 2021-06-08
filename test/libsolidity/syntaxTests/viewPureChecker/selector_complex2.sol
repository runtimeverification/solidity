contract C {
        function f() payable public returns (C) {
        return this;
    }
    function g() pure public returns (bytes4) {
        C x = C(address(0x123));
        return x.f.selector;
    }
}
// ----
// TypeError 2524: (186-198): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
