contract C {
    string s = "doh";

    function f() public returns (string memory, string memory) {
        string memory t = "ray";
        string[3] memory x = [s, t, "mi"];
        return (x[1], x[2]);
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "ray", "mi"
