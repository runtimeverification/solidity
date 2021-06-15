contract C {
    function f() public {
        address payable addr;
        uint balance = addr.balance;
        (bool callSuc,) = addr.call("");
        (bool delegatecallSuc,) = addr.delegatecall("");
        bool sendRet = addr.send(1);
        addr.transfer(1);
        balance; callSuc; delegatecallSuc; sendRet;
    }
}
// ----
// TypeError 6198: (132-141): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6198: (181-198): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
