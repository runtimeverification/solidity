contract C {
    function f(uint[] calldata x) external {
        x.pop();
    }
}
// ----
// TypeError 4994: (66-71): Member "pop" is not available in uint[] calldata outside of storage.
