contract C {
    function f() public returns (address) {
        return msg.sender;
    }
}
// ====
// compileViaYul: also
// ----
// f() -> sender_address
