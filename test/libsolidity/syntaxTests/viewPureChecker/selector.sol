contract C {
    uint public x;
    function f() payable public {
    }
    function g() pure public returns (bytes4) {
        return this.f.selector ^ this.x.selector;
    }
    function h() view public returns (bytes4) {
        x;
        return this.f.selector ^ this.x.selector;
    }
}
// ----
// TypeError 2524: (135-150): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (153-168): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (250-265): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2524: (268-283): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
