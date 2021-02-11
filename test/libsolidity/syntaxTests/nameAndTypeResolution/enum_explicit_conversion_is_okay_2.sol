contract test {
    enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
    constructor() {
        a = uint(ActionChoices.GoStraight);
        b = uint64(ActionChoices.Sit);
    }
    uint a;
    uint64 b;
}
// ----
