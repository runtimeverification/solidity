contract C {
    function f() public pure {
        abi.encode;
        abi.encodePacked;
        abi.encodeWithSelector;
        abi.encodeWithSignature;
        abi.decode;
    }
}
// ----
// TypeError 2038: (98-120): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 1379: (130-153): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
