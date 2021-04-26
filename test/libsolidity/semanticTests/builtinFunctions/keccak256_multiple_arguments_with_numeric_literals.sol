contract c {
    function foo(uint256 a, uint16 b) public returns (bytes32 d) {
        d = keccak256(abi.encodePacked(a, b, uint8(145)));
    }
}
// ====
// compileViaYul: also
// ----
// foo(uint256,uint16): 0x0a, 0x0c -> 0x0088acd45f75907e7c560318bc1a5249850a0999c4896717b1167d05d116e6dbad
