contract C {
    uint constant a = b * c;
    uint constant b = 7;
    uint constant c = 4 + uint(keccak256(d));
    uint constant d = 2 + b;
}