contract C {
    uint256 constant a = b * c;
    uint256 constant b = 7;
    uint256 constant c = b + uint256(keccak256(abi.encodePacked(d)));
    uint256 constant d = 2 + a;
}
// ----
// TypeError 6161: (17-43): The value of the constant a has a cyclic dependency via c.
// TypeError 6161: (77-141): The value of the constant c has a cyclic dependency via d.
// TypeError 6161: (147-173): The value of the constant d has a cyclic dependency via a.
