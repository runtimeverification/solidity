contract C {
    function f() public {
        payable(this).transfer(1);
        require(payable(this).send(2));
        selfdestruct(payable(this));
        (bool success,) = address(this).delegatecall("");
        require(success);
		(success,) = address(this).call("");
        require(success);
    }
    function g() pure public {
        bytes32 x = keccak256("abc");
        bytes32 y = sha256("abc");
        address z = ecrecover(bytes32(uint256(1)), uint8(2), bytes32(uint256(3)), bytes32(uint256(4)));
        require(true);
        assert(true);
        x; y; z;
    }
    receive() payable external {}
}
// ----
// TypeError 6198: (177-203): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6198: (250-268): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
