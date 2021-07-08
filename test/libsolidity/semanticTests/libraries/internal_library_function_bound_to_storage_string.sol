library L {
    function f(string memory a) internal pure returns (string memory) {
        return a;
    }
    function g(string storage a) internal pure returns (string memory) {
        return a;
    }
}

contract C {
    using L for string;
    string s;

    function test(string calldata x) public returns (string memory, string memory) {
        s = x;
        return (s.f(), s.g());
    }
}
// ====
// compileViaYul: also
// ----
// test(string): "def" -> "def", "def"
