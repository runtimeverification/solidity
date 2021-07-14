contract C {
    function f(uint8 x, uint8 y) public returns (uint) {
        return x**y;
    }
    function g(uint x, uint y) public returns (uint) {
        return x**y;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint8,uint8): 0, 0 -> 1
// f(uint8,uint8): 0, 1 -> 0x00
// f(uint8,uint8): 0, 2 -> 0x00
// f(uint8,uint8): 0, 3 -> 0x00
// f(uint8,uint8): 1, 0 -> 1
// f(uint8,uint8): 1, 1 -> 1
// f(uint8,uint8): 1, 2 -> 1
// f(uint8,uint8): 1, 3 -> 1
// f(uint8,uint8): 2, 0 -> 1
// f(uint8,uint8): 2, 1 -> 2
// f(uint8,uint8): 2, 2 -> 4
// f(uint8,uint8): 2, 3 -> 8
// f(uint8,uint8): 3, 0 -> 1
// f(uint8,uint8): 3, 1 -> 3
// f(uint8,uint8): 3, 2 -> 9
// f(uint8,uint8): 3, 3 -> 0x1b
// f(uint8,uint8): 10, 0 -> 1
// f(uint8,uint8): 10, 1 -> 0x0a
// f(uint8,uint8): 10, 2 -> 100
// g(uint,uint): 0, 0 -> 1
// g(uint,uint): 0, 1 -> 0x00
// g(uint,uint): 0, 2 -> 0x00
// g(uint,uint): 0, 3 -> 0x00
// g(uint,uint): 1, 0 -> 1
// g(uint,uint): 1, 1 -> 1
// g(uint,uint): 1, 2 -> 1
// g(uint,uint): 1, 3 -> 1
// g(uint,uint): 2, 0 -> 1
// g(uint,uint): 2, 1 -> 2
// g(uint,uint): 2, 2 -> 4
// g(uint,uint): 2, 3 -> 8
// g(uint,uint): 3, 0 -> 1
// g(uint,uint): 3, 1 -> 3
// g(uint,uint): 3, 2 -> 9
// g(uint,uint): 3, 3 -> 0x1b
// g(uint,uint): 10, 10 -> 10000000000
// g(uint,uint): 10, 77 -> 100000000000000000000000000000000000000000000000000000000000000000000000000000
// g(uint,uint): 256, 2 -> 0x010000
// g(uint,uint): 256, 31 -> 0x0100000000000000000000000000000000000000000000000000000000000000
