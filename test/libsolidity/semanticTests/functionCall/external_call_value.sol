pragma solidity >= 0.6.0;

contract C {
    function g(uint n) external payable returns (uint, uint) {
        return (msg.value * 1000, n);
    }

    function f(uint n) public payable returns (uint, uint) {
        return this.g{value: 10}(n);
    }
}

// ====
// compileViaYul: also
// ----
// g(uint), 1000000000 wei: 4 -> 1000000000000, 4
// f(uint), 11000000000 wei: 2 -> 10000, 2
