contract C {
	function dyn(uint ptr, uint start, uint x) public returns (bytes memory a) {
		assembly {
			mstore(0, start)
			mstore(start, add(start, 1))
			return(ptr, x)
		}
	}
	function f(uint ptr, uint start, uint x) public returns (bool) {
		this.dyn(ptr, start, x);
		return true;
	}
}
// ====
// EVMVersion: >=byzantium
// revertStrings: debug
// ABIEncoderV1Only: true
// ----
// f(uint256,uint256,uint256): 0, 0x200, 0x60 -> FAILURE, 39
// f(uint256,uint256,uint256): 0, 0x20, 0x60 -> FAILURE, 40 
