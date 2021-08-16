pragma abicoder               v2;
contract C {
	function enc_packed_bytes(bytes calldata data, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encodePacked(data[start:end]);
	}
	function enc_packed_bytes_reference(bytes calldata data, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encodePacked(bytes(data[start:end]));
	}

	function enc_bytes(bytes calldata data, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encode(data[start:end]);
	}
	function enc_bytes_reference(bytes calldata data, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encode(bytes(data[start:end]));
	}

	function enc_uint256(uint256[] calldata x, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encode(x[start:end]);
	}
	function enc_uint256_reference(uint256[] calldata x, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encode(x[start:end]);
	}

	function enc_packed_uint256(uint256[] calldata x, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encodePacked(x[start:end]);
	}
	function enc_packed_uint256_reference(uint256[] calldata x, uint256 start, uint256 end) external returns (bytes memory) {
		return abi.encodePacked(x[start:end]);
	}

	function compare(bytes memory x, bytes memory y) internal {
		assert(x.length == y.length);
		for (uint i = 0; i < x.length; ++i)
			assert(x[i] == y[i]);
	}

	function test_bytes() public {
		bytes memory test = new bytes(3);
		test[0] = 0x41; test[1] = 0x42; test[2] = 0x42;
		for (uint256 i = 0; i < test.length; i++)
			for (uint256 j = i; j <= test.length; j++)
			{
				compare(this.enc_packed_bytes(test, i, j), this.enc_packed_bytes_reference(test, i, j));
				compare(this.enc_bytes(test, i, j), this.enc_bytes_reference(test, i, j));
			}
	}

	function test_uint256() public {
		uint256[] memory test = new uint256[](3);
		test[0] = 0x41; test[1] = 0x42; test[2] = 0x42;
		for (uint256 i = 0; i < test.length; i++)
			for (uint256 j = i; j <= test.length; j++)
			{
				compare(this.enc_packed_uint256(test, i, j), this.enc_packed_uint256_reference(test, i, j));
				compare(this.enc_uint256(test, i, j), this.enc_uint256_reference(test, i, j));
			}
	}
}
// ====
// compileViaYul: also
// EVMVersion: >homestead
// ----
// test_bytes() ->
// test_uint256() ->
