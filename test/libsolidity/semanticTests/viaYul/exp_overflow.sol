contract C {
    function f(uint8 x, uint8 y) public returns (uint256) {
        return x**y;
    }
    function g(uint256 x, uint256 y) public returns (uint256) {
        return x**y;
    }
}
// ====
// compileViaYul: also
// ----
// f(uint8,uint8): 2, 7 -> 0x0080
// f(uint8,uint8): 2, 8 -> FAILURE, 255
// f(uint8,uint8): 15, 2 -> 225
// f(uint8,uint8): 6, 3 -> 0x00d8
// f(uint8,uint8): 7, 2 -> 0x31
// f(uint8,uint8): 7, 3 -> FAILURE, 255
// f(uint8,uint8): 7, 4 -> FAILURE, 255
// f(uint8,uint8): 255, 31 -> FAILURE, 255
// f(uint8,uint8): 255, 131 -> FAILURE, 255
// g(uint256,uint256): 0x200000000000000000000000000000000, 1 -> 0x0200000000000000000000000000000000
// g(uint256,uint256): 0x100000000000000000000000000000010, 2 -> FAILURE, 255
// g(uint256,uint256): 0x200000000000000000000000000000000, 2 -> FAILURE, 255
// g(uint256,uint256): 0x200000000000000000000000000000000, 3 -> FAILURE, 255
// g(uint256,uint256): 255, 31 -> 400631961586894742455537928461950192806830589109049416147172451019287109375
// g(uint256,uint256): 255, 32 -> 102161150204658159326162171757797299165741800222807601117528975009918212890625
// g(uint256,uint256): 255, 33 -> FAILURE, 255
// g(uint256,uint256): 255, 131 -> FAILURE, 255
// g(uint256,uint256): 258, 31 -> 575719427506838823084316385994930914701079543089399988096291424922125729792
// g(uint256,uint256): 258, 37 -> FAILURE, 255
// g(uint256,uint256): 258, 131 -> FAILURE, 255
