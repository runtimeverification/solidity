contract C {
    uint immutable x;
    constructor() {
        x = 3;
        readX().selector;
    }

    function f() external view returns(uint)  {
        return x;
    }

    function readX() public view returns(function() external view returns(uint) _f) {
        _f = this.f;
    }
}
// ----
// TypeError 2524: (78-94): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
