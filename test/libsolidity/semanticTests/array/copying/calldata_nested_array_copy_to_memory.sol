pragma abicoder v2;

contract Test {
    struct shouldBug {
        uint[][2] deadly;
    }
    function killer(uint[][2] calldata weapon) pure external returns (shouldBug memory) {
        return shouldBug(weapon);
    }
}
// ====
// compileViaYul: also
// ----
// killer(uint[][2]): refargs { 0x01, 0x02, 1, 2, 0x01, 0x02, 1, 2 } -> refargs { 0x01, 0x02, 1, 2, 0x01, 0x02, 1, 2 }
