contract C {
    function g() internal pure returns (bytes calldata) {
        return msg.data;
    }
    function h(uint[] calldata _c) internal pure {
        uint[] calldata c;
        c = _c;
        c[2];
    }
    function i(uint[] calldata _c) internal pure {
        uint[] calldata c;
        (c) = _c;
        c[2];
    }
}
// ----
// TypeError 2699: (86-94): msg.data is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
