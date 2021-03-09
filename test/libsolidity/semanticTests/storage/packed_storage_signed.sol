contract C {
    int8 a;
    uint8 b;
    int8 c;
    uint8 d;

    function test()
        public
        returns (uint x1, uint x2, uint x3, uint x4)
    {
        a = -2;
        unchecked {
            b = (0 - uint8(a)) * 2;
            c = a * int8(120) * int8(121);
        }
        x1 = uint(int(a));
        x2 = b;
        x3 = uint(int(c));
        x4 = d;
    }
}

// ====
// compileViaYul: also
// ----
// test() ->
