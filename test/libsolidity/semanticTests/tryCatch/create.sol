contract Reverts {
    constructor(uint) { revert("test message."); }
}
contract Succeeds {
    constructor(uint) { }
}

contract C {
    function f() public returns (Reverts x, uint, string memory txt) {
        uint i = 3;
        try new Reverts(i) returns (Reverts r) {
            x = r;
            txt = "success";
        } catch Error(string memory s) {
            txt = s;
        }
    }
    function g() public returns (bool x, uint, string memory txt) {
        uint i = 8;
        try new Succeeds(i) returns (Succeeds r) {
            x = address(r) != address(0);
            txt = "success";
        } catch Error(string memory s) {
            txt = s;
        }
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=byzantium
// ----
// f() -> 0, 0, "test message."
// g() -> true, 0, "success"
