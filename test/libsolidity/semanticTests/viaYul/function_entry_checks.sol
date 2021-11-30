contract C {
    function f() public returns (uint) {
    }
    function g(uint x, uint y) public returns (uint) {
    }
    function h() public payable returns (uint) {
    }
    function i(bytes32 b) public returns (bytes32) {
    }
    function j(bool b) public returns (bool) {
    }
    function k(bytes32 b) public returns (bytes32) {
    }
    function s(bytes memory b) public returns (uint[] memory) {
    }
    function t(uint) public pure {
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> 0
// g(uint,uint): 1, 0x00fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffe -> 0
// h(), 1 wei -> 0
// i(bytes32), 1 wei: 2 -> FAILURE, 255
// i(bytes32): 2 -> 0
// j(bool): true -> false
// k(bytes32): 0x31 -> 0x00
// s(bytes): hex"4200ef" -> 1
// t(uint) -> FAILURE, 2
