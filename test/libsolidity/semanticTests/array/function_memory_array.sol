contract C {
    function a(uint x) public returns (uint) {
        return x + 1;
    }

    function b(uint x) public returns (uint) {
        return x + 2;
    }

    function c(uint x) public returns (uint) {
        return x + 3;
    }

    function d(uint x) public returns (uint) {
        return x + 5;
    }

    function e(uint x) public returns (uint) {
        return x + 8;
    }

    function test(uint x, uint i) public returns (uint) {
        function(uint) internal returns (uint)[] memory arr =
            new function(uint) internal returns (uint)[](10);
        arr[0] = a;
        arr[1] = b;
        arr[2] = c;
        arr[3] = d;
        arr[4] = e;
        return arr[i](x);
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// test(uint,uint): 10, 0 -> 11
// test(uint,uint): 10, 1 -> 12
// test(uint,uint): 10, 2 -> 13
// test(uint,uint): 10, 3 -> 15
// test(uint,uint): 10, 4 -> 18
// test(uint,uint): 10, 5 -> FAILURE, 1
