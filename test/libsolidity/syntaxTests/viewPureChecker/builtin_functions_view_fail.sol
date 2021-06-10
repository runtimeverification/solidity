contract C {
    function f() view public {
        payable(this).transfer(1);
    }
    function g() view public {
        require(payable(this).send(2));
    }
    function h() view public {
        selfdestruct(payable(this));
    }
    function i() view public {
        (bool success,) = address(this).delegatecall("");
        require(success);
    }
    function j() view public {
        (bool success,) = address(this).call("");
        require(success);
    }
    receive() payable external {
    }
}
// ----
// TypeError 6198: (293-319): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6198: (414-432): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
