contract c {
    function bignum() public {
        uint a;
        a = 134562324532464234452335168163516E1200 / 134562324532464234452335168163516E1200; // still fine
        a = 1345623245324642344523351681635168E1200; // too large
    }
}
// ----
// Warning: (17-238): Function state mutability can be restricted to pure
