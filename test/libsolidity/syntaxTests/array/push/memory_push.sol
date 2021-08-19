contract C {
    function f() public {
        uint[] memory x;
        x.push();
    }
}
// ----
// TypeError 4994: (72-78): Member "push" is not available in uint[] memory outside of storage.
