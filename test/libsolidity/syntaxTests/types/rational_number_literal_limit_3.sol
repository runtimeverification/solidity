contract c {
    function bignum() public {
        uint256 a;
        a = 134562324532464.234452335168163517E1200 / 134562324532464.234452335168163517E1200; // still fine
        a = 134562324532464.2344523351681635177E1200; // too large
    }
}
// ----
// TypeError 7407: (184-224): Type int_const 1345...(1207 digits omitted)...0000 is not implicitly convertible to expected type uint256. Literal is too large to fit in uint256.
