contract CalldataTest {
    function test(bytes calldata x) public returns (bytes calldata) {
        return x;
    }
    function tester(bytes calldata x) public returns (bytes1) {
        return this.test(x)[2];
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// tester(bytes): "abcdefgh" -> 0x63
