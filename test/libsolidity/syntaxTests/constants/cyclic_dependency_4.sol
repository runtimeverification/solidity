contract C {
    uint256 constant a = b * c;
    uint256 constant b = 7;
    uint256 constant c = 4 + uint256(keccak256(abi.encode(d)));
    uint256 constant d = 2 + b;
}
// ----
