contract C {
    function f() public payable returns (uint) {
        return msg.value;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> 0
// f(), 12000000000 wei -> 12000000000
