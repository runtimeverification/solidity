contract Test {
    string s;
    bytes b;

    function f(string memory _s, uint256 n) public returns (bytes1) {
        b = bytes(_s);
        s = string(b);
        return bytes(s)[n];
    }

    function l() public returns (uint) {
        return bytes(s).length;
    }
}
// ====
// compileViaYul: also
// ----
// f(string,uint256): "abcdef", 0x02 -> 0x63
// l() -> 0x06
