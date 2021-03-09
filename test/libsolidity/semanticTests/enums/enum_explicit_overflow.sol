contract test {
    enum ActionChoices {GoLeft, GoRight, GoStraight}

    constructor() {}

    function getChoiceExp(uint x) public returns (uint d) {
        choice = ActionChoices(x);
        d = uint(choice);
    }

    function getChoiceFromSigned(int x) public returns (uint d) {
        choice = ActionChoices(x);
        d = uint(choice);
    }

    function getChoiceFromMax() public returns (uint d) {
        choice = ActionChoices(type(uint).max);
        d = uint(choice);
    }

    ActionChoices choice;
}

// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// getChoiceExp(uint): 2 -> 2
// getChoiceExp(uint): 3 -> FAILURE, hex"4e487b71", 33 # These should throw #
// getChoiceFromSigned(int): -1 -> FAILURE, hex"4e487b71", 33
// getChoiceFromMax() -> FAILURE, hex"4e487b71", 33
// getChoiceExp(uint): 2 -> 2 # These should work #
// getChoiceExp(uint): 0 -> 0
