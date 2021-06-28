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
        choice = ActionChoices(type(uint256).max);
        d = uint(choice);
    }

    ActionChoices choice;
}

// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// getChoiceExp(uint): 2 -> 2
// getChoiceExp(uint): 3 -> FAILURE, 255 # These should throw #
// getChoiceFromSigned(int): -1 -> FAILURE, 255
// getChoiceFromMax() -> FAILURE, 255
// getChoiceExp(uint): 2 -> 2 # These should work #
// getChoiceExp(uint): 0 -> 0
