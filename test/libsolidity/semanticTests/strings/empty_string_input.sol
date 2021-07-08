contract C {
    function f() public pure returns (string memory) {
        return "";
    }
    function g(string calldata msg) public pure returns (string memory) {
        return msg;
    }
    function h(string calldata msg, uint256 v) public pure returns (string memory, uint256) {
        return (msg, v);
    }
    // Adjusting order of input/output intentionally.
    function i(string calldata msg1, uint256 v, string calldata msg2) public pure returns (string memory, string memory, uint256) {
        return (msg1, msg2, v);
    }
    function j(string calldata msg1, uint256 v) public pure returns (string memory, string memory, uint256) {
        return (msg1, "", v);
    }
}
// ====
// compileViaYul: also
// ----
// f() -> ""
// g(string): "" -> ""
// h(string,uint256): "", 0x888 -> "", 0x0888
// i(string,uint256,string): "", 0x888, "" -> "", "", 0x0888
// j(string,uint256): "", 0x888 -> "", "", 0x0888
