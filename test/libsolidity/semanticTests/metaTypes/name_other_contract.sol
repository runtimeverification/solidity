abstract contract A {
    function f() virtual public pure;
}

interface I {
    function f() external pure;
}

contract C {
    function f() pure public {
    }
}

contract Test is C {
    function c() public pure returns (string memory) {
        return type(C).name;
    }
    function a() public pure returns (string memory) {
        return type(A).name;
    }
    function i() public pure returns (string memory) {
        return type(I).name;
    }
}

// ----
// c() -> "C"
// a() -> "A"
// i() -> "I"
