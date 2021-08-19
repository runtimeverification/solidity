// Tests that this will not end up using a "bytes0" type
// (which would assert)
pragma abicoder               v2;


contract C {
    function f() public pure returns (bytes memory, bytes memory) {
        return (abi.encode(""), abi.encodePacked(""));
    }
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> "\x00\x00\x00\x00\x00\x00\x00\x00", ""
