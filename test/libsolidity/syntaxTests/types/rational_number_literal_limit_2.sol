contract c {
    function bignum() public {
        uint256 a;
        a = 134562324532464234452335168163516E1200 / 134562324532464234452335168163516E1200; // still fine
        a = 1345623245324642344523351681635168E1200; // too large
    }
}
// ----
<<<<<<< ours
// TypeError: (182-221): Type int_const 1345...(1226 digits omitted)...0000 is not implicitly convertible to expected type uint256.
=======
// TypeError 2826: (179-218): Invalid literal value.
>>>>>>> theirs
