pragma solidity >= 0.6.0;

contract C {
    function h(uint[4] memory n) public pure returns (uint) {
        return n[0] + n[1] + n[2] + n[3];
    }

    function i(uint[4] memory n) public view returns (uint) {
        return this.h(n) * 2;
    }
}

// ====
// compileViaYul: also
// ----
// h(uint[4]): array 0 [ 1, 2, 3, 4 ] -> 10
// i(uint[4]): array 0 [ 1, 2, 3, 4 ] -> 20
