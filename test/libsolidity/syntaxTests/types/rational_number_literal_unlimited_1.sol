contract c {
    function bignum() public {
        uint a;
        a = 1e1233 / 1e1233; // 1e1233 is still fine
        a = 1e1234; // 1e1234 is too big
    }
}
// ----
// Warning 2018: (17-159): Function state mutability can be restricted to pure
