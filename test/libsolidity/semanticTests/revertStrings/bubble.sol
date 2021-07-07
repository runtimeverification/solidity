contract A {
	function g() public { revert("fail"); }
}

contract C {
	A a = new A();
	function f() public {
		a.g();
	}
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// revertStrings: debug
// ----
// f() -> FAILURE, 0x6c696166
