contract C {
    function g(bool b) public pure returns (uint, uint) {
        require(b, "message");
        return (1, 2);
    }
    function f(bool b) public returns (uint x, uint y, string memory txt) {
        try this.g(b) returns (uint a, uint b) {
            (x, y) = (a, b);
            txt = "success";
        } catch Error(string memory s) {
            txt = s;
        }
    }
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: also
// ----
// f(bool): true -> 1, 2, "success"
// f(bool): false -> 0, 0, "message"
