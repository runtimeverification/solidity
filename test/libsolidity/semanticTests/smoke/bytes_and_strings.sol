contract C {
    function e(bytes memory b) public pure returns (bytes memory) {
        return b;
    }
    function f() public pure returns (string memory, string memory) {
        return ("any", "any");
    }
    function g() public pure returns (string memory, uint, string memory) {
        return ("any", 42, "any");
    }
    function h() public pure returns (string memory) {
        return "any";
    }
}
// ====
// compileViaYul: also
// ----
// e(bytes): "\xAB\x33\xBB" -> "\xAB\x33\xBB"
// e(bytes): "\x20" -> "\x20"
// e(bytes): "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20" -> "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
// e(bytes): "\xAB\x33\xFF" -> "\xAB\x33\xFF"
// f() -> "any", "any"
// g() -> "any", 0x2a, "any"
// h() -> "any"

