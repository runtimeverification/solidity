pragma abicoder               v2;

contract C {
    struct T {
        uint a;
        uint b;
        string s;
    }
    bool[2][] flags;
    function r() public returns (bool[3] memory) {
        return [true, false, true];
    }
    function s() public returns (uint[2] memory, uint) {
        return ([uint(123), 456], 789);
    }
    function u() public returns (T[2] memory) {
        return [T(23, 42, "any"), T(555, 666, "any")];
    }
    function v() public returns (bool[2][] memory) {
        return flags;
    }
    function w1() public returns (string[1] memory) {
        return ["any"];
    }
    function w2() public returns (string[2] memory) {
        return ["any", "any"];
    }
    function w3() public returns (string[3] memory) {
        return ["any", "any", "any"];
    }
    function x() public returns (string[2] memory, string[3] memory) {
        return (["any", "any"], ["any", "any", "any"]);
    }
}
// ====
// compileViaYul: also
// ----
// r() -> refargs { true, false, true }
// s() -> array 0 [ 123, 456 ], 789
// u() -> refargs { 23, 42, "any", 555, 666, "any" }
// v() -> refargs { 0x01 }
// w1() -> refargs { "any" }
// w2() -> refargs { "any", "any" }
// w3() -> refargs { "any", "any", "any" }
// x() -> refargs { "any", "any" }, refargs { "any", "any", "any" }
