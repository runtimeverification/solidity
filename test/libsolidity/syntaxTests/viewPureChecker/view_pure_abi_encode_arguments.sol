contract C {
    uint x;
    function gView() public view returns (uint) { return x; }
    function gNonPayable() public returns (uint) { x = 4; return 0; }

    function f1() view public returns (bytes memory) {
        return abi.encode(gView());
    }
    function f2() view public returns (bytes memory) {
        return abi.encodePacked(gView());
    }
    function g1() public returns (bytes memory) {
        return abi.encode(gNonPayable());
    }
    function g2() public returns (bytes memory) {
        return abi.encodePacked(gNonPayable());
    }
    // This will generate the only warning.
    function check() public returns (bytes memory) {
        return abi.encode(2);
    }
}
// ----
// Warning 2018: (608-692): Function state mutability can be restricted to pure
