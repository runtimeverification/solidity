contract C {
    uint[] storageArray;
    function test_boundary_check(uint len, uint access) public returns (uint)
    {
        while(storageArray.length < len)
            storageArray.push();
        while(storageArray.length > len)
            storageArray.pop();
        return storageArray[access];
    }
}
// ====
// compileViaYul: also
// ----
// test_boundary_check(uint,uint): 10, 11 -> FAILURE, 255
// test_boundary_check(uint,uint): 10, 9 -> 0
// test_boundary_check(uint,uint): 1, 9 -> FAILURE, 255
// test_boundary_check(uint,uint): 1, 1 -> FAILURE, 255
// test_boundary_check(uint,uint): 10, 10 -> FAILURE, 255
// test_boundary_check(uint,uint): 256, 256 -> FAILURE, 255
// test_boundary_check(uint,uint): 256, 255 -> 0
// test_boundary_check(uint,uint): 256, 0xFFFF -> FAILURE, 255
// test_boundary_check(uint,uint): 256, 2 -> 0
