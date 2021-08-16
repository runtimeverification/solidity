contract C {
	function f(uint256 start, uint256 end, uint256[] calldata arr) external pure {
		arr[start:end];
	}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// compileViaYul: also
// ----
// f(uint256,uint256,uint256[]): 2, 1, dynarray 256 [ 1, 2, 3 ] -> FAILURE, 255
// f(uint256,uint256,uint256[]): 1, 5, dynarray 256 [ 1, 2, 3 ] -> FAILURE, 255
