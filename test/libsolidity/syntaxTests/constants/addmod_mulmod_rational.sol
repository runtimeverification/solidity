contract C {
    uint constant a = addmod(3, 4, 0.1);
    uint constant b = mulmod(3, 4, 0.1);
}
// ----
<<<<<<< ours
// TypeError: (48-51): Invalid type for argument in function call. Invalid implicit conversion from rational_const 1 / 10 to uint requested.
// TypeError: (89-92): Invalid type for argument in function call. Invalid implicit conversion from rational_const 1 / 10 to uint requested.
=======
// TypeError 9553: (48-51): Invalid type for argument in function call. Invalid implicit conversion from rational_const 1 / 10 to uint256 requested.
// TypeError 9553: (89-92): Invalid type for argument in function call. Invalid implicit conversion from rational_const 1 / 10 to uint256 requested.
>>>>>>> theirs
