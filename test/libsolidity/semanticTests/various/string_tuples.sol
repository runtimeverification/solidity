contract C {
    function f() public returns (string memory, uint256) {
        return ("abc", 8);
    }

    function g() public returns (string memory, string memory) {
        return (h(), "def");
    }

    function h() public returns (string memory) {
        return ("abc");
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "abc", 0x8
// g() -> "abc", "def"
