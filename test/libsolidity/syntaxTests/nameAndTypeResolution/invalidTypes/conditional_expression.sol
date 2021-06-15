contract C {
    function o(bytes1) public pure {}
    function f() public {
        o(true ? 99**99 : 99);
        o(true ? 99 : 99**99);

        o(true ? 99**99 : 99**99);
    }
}
// ----
// TypeError 9553: (87-105): Invalid type for argument in function call. Invalid implicit conversion from uint to bytes1 requested.
// TypeError 9553: (118-136): Invalid type for argument in function call. Invalid implicit conversion from uint to bytes1 requested.
// TypeError 9553: (150-172): Invalid type for argument in function call. Invalid implicit conversion from uint to bytes1 requested.
