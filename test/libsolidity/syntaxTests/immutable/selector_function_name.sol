contract C {
    uint immutable x;
    constructor() {
        x = 3;
        C.selector;
    }

    function selector() external view returns(uint) { return x; }
}
// ----
// Warning 6133: (78-88): Statement has no effect.
