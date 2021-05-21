contract C {
	uint[] r;
    function f() internal view returns (uint[] storage s) {
        assembly { pop(s.slot) }
        s = r;
    }
}
// ----
// SyntaxError 1184: (92-116): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
