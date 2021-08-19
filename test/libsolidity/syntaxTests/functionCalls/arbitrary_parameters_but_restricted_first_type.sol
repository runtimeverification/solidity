contract C {
    function f() pure public {
        abi.encodeWithSelector();
        abi.encodeWithSignature();
        abi.encodeWithSelector(uint(2), 2);
        abi.encodeWithSignature(uint(2), 2);
    }
}
// ----
// TypeError 2038: (52-74): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 9308: (52-76): Need at least 1 arguments for function call, but provided only 0.
// TypeError 1379: (86-109): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 9308: (86-111): Need at least 1 arguments for function call, but provided only 0.
// TypeError 2038: (121-143): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 9553: (144-151): Invalid type for argument in function call. Invalid implicit conversion from uint to bytes4 requested.
// TypeError 1379: (165-188): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 9553: (189-196): Invalid type for argument in function call. Invalid implicit conversion from uint to string memory requested.
