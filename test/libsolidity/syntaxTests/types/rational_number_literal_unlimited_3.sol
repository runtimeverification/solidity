contract c {
    function bignum() public {
        uint a;
        a = 134562324532464.234452335168163517E1200 / 134562324532464.234452335168163517E1200; // still fine
        a = 134562324532464.2344523351681635177E1200; // too large
    }
}
// ----
// Warning 2018: (17-241): Function state mutability can be restricted to pure
