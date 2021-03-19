contract Base {
    uint dataBase;

    function getViaBase() public returns (uint i) {
        return dataBase;
    }
}


contract Derived is Base {
    uint dataDerived;

    function setData(uint base, uint derived) public returns (bool r) {
        dataBase = base;
        dataDerived = derived;
        return true;
    }

    function getViaDerived() public returns (uint base, uint derived) {
        base = dataBase;
        derived = dataDerived;
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// setData(uint,uint): 1, 2 -> true
// getViaBase() -> 1
// getViaDerived() -> 1, 2
