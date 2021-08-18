contract A {
	receive() external payable {
		revert("no_receive");
	}
}

contract C {
	A a = new A();
	receive() external payable {}
	function f() public {
		payable(a).transfer(1 wei);
	}
	function h() public {
		payable(a).transfer(100 ether);
	}
	function g() public view returns (uint) {
		return payable(this).balance;
	}
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// revertStrings: debug
// ----
// (), 10 wei ->
// g() -> 10
// f() -> FAILURE, "no_receive"
// h() -> FAILURE, 7
