contract A {
    uint data;
    receive() external payable { ++data; }
    function getData() public returns (uint r) { return data; }
}
contract B is A {}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// getData() -> 0
// () ->
// getData() -> 1
// (), 1 wei ->
// getData() -> 2
