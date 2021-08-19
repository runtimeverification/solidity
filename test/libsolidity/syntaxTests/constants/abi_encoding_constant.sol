contract C {
    bytes32 constant a = keccak256(abi.encode(1, 2));
    bytes32 constant b = keccak256(abi.encodePacked(uint(1), a));
    bytes32 constant c = keccak256(abi.encodeWithSelector(0x12345678, b, 2));
    bytes32 constant d = keccak256(abi.encodeWithSignature("f()", 1, 2));
}
// ----
// TypeError 2038: (168-190): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 1379: (246-269): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
