contract C {
    function f1(bytes calldata c1, uint256 s, uint256 e, bytes calldata c2) public returns (bool) {
        return keccak256(c1[s:e]) == keccak256(c2);
    }

    function f2(bytes calldata c, uint256 s) public returns (uint256, bytes memory) {
        return abi.decode(c[s:], (uint256, bytes));
    }

    function f3(bytes calldata c1, uint256 s, uint256 e, bytes calldata c2) public returns (bool) {
        bytes memory a = abi.encode(c1[s:e]);
        bytes memory b = abi.encode(c2);
        if (a.length != b.length) { return false; }
        for (uint256 i = 0; i < a.length; i++) {
            if (a[i] != b[i]) { return false; }
        }
        return true;
    }

    function f4(bytes calldata c1, uint256 s, uint256 e, bytes calldata c2) public returns (bool) {
        bytes memory a = abi.encodePacked(c1[s:e]);
        bytes memory b = abi.encodePacked(c2);
        if (a.length != b.length) { return false; }
        for (uint256 i = 0; i < a.length; i++) {
            if (a[i] != b[i]) { return false; }
        }
        return true;
    }
}
// ====
// compileViaYul: also
// ----
// f1(bytes,uint256,uint256,bytes): "abcdefgh", 1, 5, "bcde" -> true
// f1(bytes,uint256,uint256,bytes): "abcdefgh", 1, 5, "bcdf" -> false
// f2(bytes,uint256): "\x21\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x07\x00\x00\x00\x00\x00\x00\x00gfedcba", 0 -> 0x21, "abcdefg"
// f3(bytes,uint256,uint256,bytes): "abcdefgh", 1, 5, "bcde" -> true
// f3(bytes,uint256,uint256,bytes): "abcdefgh", 1, 5, "bcdf" -> false
// f4(bytes,uint256,uint256,bytes): "abcdefgh", 1, 5, "bcde" -> true
// f4(bytes,uint256,uint256,bytes): "abcdefgh", 1, 5, "bcdf" -> false
