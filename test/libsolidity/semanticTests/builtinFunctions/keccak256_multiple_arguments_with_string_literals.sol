contract c {
    function foo() public returns (bytes32 d) {
        d = keccak256("foo");
    }

    function bar(uint a, uint16 b) public returns (bytes32 d) {
        d = keccak256(abi.encodePacked(a, b, uint8(145), "foo"));
    }
}
// ====
// compileViaYul: also
// ----
// foo() -> 0x0041b1a0649752af1b28b3dc29a1556eee781e4a4c3a1f7f53f90fa834de098c4d
// bar(uint,uint16): 0xa, 0xc -> 0x006990f36476dc412b1c4baa48e2d9f4aa4bb313f61fda367c8fdbbb2232dc6146
