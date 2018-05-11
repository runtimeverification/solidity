contract C {
    uint x;
    function gView() public view returns (uint) { return x; }
    function gNonPayable() public returns (uint) { x = 4; return 0; }

    function f1() view public returns (bytes) {
        return abi.encode(gView());
    }
    function f2() view public returns (bytes) {
        return abi.encodePacked(gView());
    }
    function f3() view public returns (bytes) {
        return abi.encodeWithSelector(0x12345678, gView());
    }
    function f4() view public returns (bytes) {
        return abi.encodeWithSignature("f(uint256)", gView());
    }
    function g1() public returns (bytes) {
        return abi.encode(gNonPayable());
    }
    function g2() public returns (bytes) {
        return abi.encodePacked(gNonPayable());
    }
    function g3() public returns (bytes) {
        return abi.encodeWithSelector(0x12345678, gNonPayable());
    }
    function g4() public returns (bytes) {
        return abi.encodeWithSignature("f(uint256)", gNonPayable());
    }
    // This will generate the only warning.
    function check() public returns (bytes) {
        return abi.encode(2);
    }
}
// ----
// TypeError: (407-450): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (521-567): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (821-870): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (936-988): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
