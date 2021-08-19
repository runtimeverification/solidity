==== Source: s1.sol ====


bytes constant a = b;
bytes constant b = hex"030102";

function fre() pure returns (bytes memory) {
    return a;
}

==== Source: s2.sol ====

import "s1.sol";

uint256 constant c = uint8(a[0]) + 2;

contract C {
    function f() public returns (bytes memory) {
        return a;
    }

    function g() public returns (bytes memory) {
        return b;
    }

    function h() public returns (uint) {
        return c;
    }

    function i() public returns (bytes memory) {
        return fre();
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "\x03\x01\x02"
// g() -> "\x03\x01\x02"
// h() -> 5
// i() -> "\x03\x01\x02"
