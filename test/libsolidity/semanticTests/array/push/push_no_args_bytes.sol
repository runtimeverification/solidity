contract C {
	bytes array;

	function f() public {
		array.push();
	}

	function g(uint x) public {
		for (uint i = 0; i < x; ++i)
			array.push() = bytes1(uint8(i));
	}

	function l() public returns (uint) {
		return array.length;
	}

	function a(uint index) public view returns (bytes1) {
		return array[index];
	}
}
// ====
// compileViaYul: also
// ----
// l() -> 0
// g(uint): 70 ->
// l() -> 70
// a(uint): 69 -> left(69)
// f() ->
// l() -> 71
// a(uint): 70 -> 0
