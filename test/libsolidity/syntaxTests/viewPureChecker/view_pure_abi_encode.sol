contract C {
    function f() pure public returns (bytes memory r) {
        r = abi.encode(1, 2);
        r = abi.encodePacked(f());
        r = abi.encodeWithSelector(0x12345678, 1);
        r = abi.encodeWithSignature("f(uint256)", 4);
    }
}
// ----
// TypeError 2038: (146-183): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 1379: (197-237): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
