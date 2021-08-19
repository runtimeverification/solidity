contract C {
    function exp_2(uint256 y) public returns (uint256) {
        return uint256(2)**y;
    }
    function exp_minus_2(uint256 y) public returns (int256) {
        return int256(-2)**y;
    }

    function exp_uint_max(uint256 y) public returns (uint256) {
        return uint256(2**256 - 1)**y;
    }
    function exp_int_max(uint256 y) public returns (int256) {
        return int256((-2)**255)**y;
    }

    function exp_5(uint256 y) public returns (uint256) {
        return uint256(5)**y;
    }
    function exp_minus_5(uint256 y) public returns (int256) {
        return int256(-5)**y;
    }

    function exp_256(uint256 y) public returns (uint256) {
        return uint256(256)**y;
    }
    function exp_minus_256(uint256 y) public returns (int256) {
        return int256(-256)**y;
    }

}
// ====
// compileViaYul: true
// compileToEwasm: also
// ----
// exp_2(uint256): 255 -> 57896044618658097711785492504343953926634992332820282019728792003956564819968
// exp_2(uint256): 256 -> FAILURE, 255
// exp_minus_2(uint256): 255 -> -57896044618658097711785492504343953926634992332820282019728792003956564819968
// exp_minus_2(uint256): 256 -> FAILURE, 255
// exp_uint_max(uint256): 1 -> 115792089237316195423570985008687907853269984665640564039457584007913129639935
// exp_uint_max(uint256): 2 -> FAILURE, 255
// exp_int_max(uint256): 1 -> -57896044618658097711785492504343953926634992332820282019728792003956564819968
// exp_int_max(uint256): 2 -> FAILURE, 255
// exp_5(uint256): 110 -> 77037197775489434122239117703397092741524065928615527809597551822662353515625
// exp_5(uint256): 111 -> FAILURE, 255
// exp_minus_5(uint256): 109 -> -15407439555097886824447823540679418548304813185723105561919510364532470703125
// exp_minus_5(uint256): 110 -> FAILURE, 255
// exp_256(uint256): 31 -> 452312848583266388373324160190187140051835877600158453279131187530910662656
// exp_256(uint256): 32 -> FAILURE, 255
// exp_minus_256(uint256): 31 -> -452312848583266388373324160190187140051835877600158453279131187530910662656
// exp_minus_256(uint256): 32 -> FAILURE, 255
