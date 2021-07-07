contract C {
    function f() public pure returns (string memory) {
        return unicode"😃, 😭, and 😈";
    }
    function g() public pure returns (string memory) {
        return unicode"😃, 😭,\
 and 😈";
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "\xf0\x9f\x98\x83, \xf0\x9f\x98\xad, and \xf0\x9f\x98\x88"
// g() -> "\xf0\x9f\x98\x83, \xf0\x9f\x98\xad, and \xf0\x9f\x98\x88"
