contract C {
	function f1() external pure returns (string memory) { return "abcabc"; }
	function f2() external pure returns (string memory) { return "abcabc`~12345677890- _=+!@#$%^&*()[{]}|;:',<.>?"; }
	function g() external pure returns (bytes32) { return "abcabc"; }
	function h() external pure returns (bytes4) { return 0xcafecafe; }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f1() -> "abcabc"
// f2() -> "abcabc`~12345677890- _=+!@#$%^&*()[{]}|;:',<.>?"
// g() -> 0x6162636162630000000000000000000000000000000000000000000000000000
// h() -> 0x00cafecafe
