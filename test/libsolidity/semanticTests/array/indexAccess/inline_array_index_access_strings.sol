contract C {
    string public tester;

    function f() public returns (string memory) {
        return (["abc", "def", "g"][0]);
    }

    function test() public {
        tester = f();
    }
}

// ----
// test() ->
// tester() -> "abc"
