contract C {
    function (uint) external returns (uint) x;
    function f() public {
        x{value: 2}(1);
    }
}
// ----
<<<<<<< ours
// TypeError: (94-101): Member "value" not found or not visible after argument-dependent lookup in function (uint) external returns (uint) - did you forget the "payable" modifier?
=======
// TypeError 7006: (94-105): Cannot set option "value" on a non-payable function type.
>>>>>>> theirs
