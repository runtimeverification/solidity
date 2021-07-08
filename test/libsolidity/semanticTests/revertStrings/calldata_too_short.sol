contract C {
	function d(bytes memory _data) public pure returns (uint8) {
		return abi.decode(_data, (uint8));
	}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// ABIEncoderV1Only: true
// ----
// d(bytes): 0x20, 0x01, 0x0000000000000000000000000000000000000000000000000000000000000000 -> FAILURE, 18 
