contract C {
    uint immutable x;
    constructor() {
        x = 3;
        this.readX.selector;
    }

    function readX() external view returns(uint) { return x; }
}
// ----
// TypeError 2524: (78-97): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
