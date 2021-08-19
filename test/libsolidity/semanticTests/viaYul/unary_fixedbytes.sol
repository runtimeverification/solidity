contract C {
	function conv(bytes25 a) public pure returns (bytes32) {
		// truncating and widening
		return ~bytes32(bytes16(~a));
	}

	function upcast(bytes25 a) public pure returns (bytes32) {
		// implicit widening is allowed
		return ~a;
	}

	function downcast(bytes25 a) public pure returns (bytes12) {
		// truncating cast must be explicit
		return bytes12(~a);
	}

	function r_b32() public pure returns (bytes32) {
		return ~bytes32(hex"ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00");
	}
	function r_b25() public pure returns (bytes25) {
		return ~bytes25(hex"ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff");
	}
	function r_b16() public pure returns (bytes16) {
		return ~bytes16(hex"ff00ff00ff00ff00ff00ff00ff00ff00");
	}
	function r_b8() public pure returns (bytes8) {
		return ~bytes8(hex"ff00ff00ff00ff00");
	}
	function r_b4() public pure returns (bytes4) {
		return ~bytes4(hex"ff00ff00");
	}
	function r_b1() public pure returns (bytes1) {
		return ~bytes1(hex"55");
	}

	function a_b32() public pure returns (bytes32) {
		bytes32 r = ~bytes32(hex"ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00");
		return r;
	}
	function a_b25() public pure returns (bytes25) {
		bytes25 r = ~bytes25(hex"ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff");
		return r;
	}
	function a_b16() public pure returns (bytes16) {
		bytes16 r =  ~bytes16(hex"ff00ff00ff00ff00ff00ff00ff00ff00");
		return r;
	}
	function a_b8() public pure returns (bytes8) {
		bytes8 r =  ~bytes8(hex"ff00ff00ff00ff00");
		return r;
	}
	function a_b4() public pure returns (bytes4) {
		bytes4 r =  ~bytes4(hex"ff00ff00");
		return r;
	}
	function a_b1() public pure returns (bytes1) {
		bytes1 r =  ~bytes1(hex"55");
		return r;
	}
}
// ====
// compileViaYul: also
// ----
// conv(bytes25): 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff -> 0x00ff00ff00ff00ff00ff00ff00ff00ff00ffffffffffffffffffffffffffffffff
// upcast(bytes25): 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff -> 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff0000000000000000
// downcast(bytes25): 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff -> 0x00ff00ff00ff00ff00ff00ff
// r_b32() -> 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff
// r_b25() -> 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00
// r_b16() -> 0x00ff00ff00ff00ff00ff00ff00ff00ff
// r_b8() -> 0x00ff00ff00ff00ff
// r_b4() -> 0x00ff00ff
// r_b1() -> 0x00aa
// a_b32() -> 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff
// a_b25() -> 0x00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00ff00
// a_b16() -> 0x00ff00ff00ff00ff00ff00ff00ff00ff
// a_b8() -> 0x00ff00ff00ff00ff
// a_b4() -> 0x00ff00ff
// a_b1() -> 0x00aa
