contract C {
	enum E {X, Y}
	function f(E[] calldata arr) external {
		arr[1];
	}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// ABIEncoderV1Only: true
// ----
// f(uint8[]): refargs { 2, 3, 3 } -> FAILURE, 255
