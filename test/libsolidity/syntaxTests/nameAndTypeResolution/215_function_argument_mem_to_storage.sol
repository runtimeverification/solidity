contract C {
    function f(uint[] storage x) private {
    }
    function g(uint[] memory x) public {
        f(x);
    }
}
// ----
// TypeError 9553: (113-114): Invalid type for argument in function call. Invalid implicit conversion from uint[] memory to uint[] storage pointer requested.
