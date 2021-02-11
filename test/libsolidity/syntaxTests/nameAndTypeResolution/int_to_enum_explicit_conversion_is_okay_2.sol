contract test {
    enum ActionChoices { GoLeft, GoRight, GoStraight, Sit }
    constructor() {
        a = 2;
        b = ActionChoices(a);
    }
    uint a;
    ActionChoices b;
}
// ----
