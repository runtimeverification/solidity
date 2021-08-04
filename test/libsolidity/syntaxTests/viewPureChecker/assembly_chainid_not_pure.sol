contract C {
    function g() public pure returns (uint) {
        return block.chainid;
    }
}
// ====
// EVMVersion: >=istanbul
// ----
// TypeError 2527: (74-87): Function declared as pure, but this expression (potentially) reads from the environment or state and thus requires "view".
