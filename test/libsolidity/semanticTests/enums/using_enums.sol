contract test {
    enum ActionChoices {GoLeft, GoRight, GoStraight, Sit}

    constructor() {
        choices = ActionChoices.GoStraight;
    }

    function getChoice() public returns (uint d) {
        d = uint(choices);
    }

    ActionChoices choices;
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// getChoice() -> 2
