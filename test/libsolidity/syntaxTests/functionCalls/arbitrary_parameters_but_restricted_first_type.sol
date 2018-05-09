contract C {
    function f() pure public {
        abi.encodeWithSelector();
        abi.encodeWithSignature();
        abi.encodeWithSelector(uint(2), 2);
        abi.encodeWithSignature(uint(2), 2);
    }
}
// ----
// TypeError: (52-76): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (52-76): Need at least 1 arguments for function call, but provided only 0.
// TypeError: (86-111): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (86-111): Need at least 1 arguments for function call, but provided only 0.
// TypeError: (121-155): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (144-151): Invalid type for argument in function call. Invalid implicit conversion from uint to bytes4 requested.
// TypeError: (165-200): abi.encodeWithSignature not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (189-196): Invalid type for argument in function call. Invalid implicit conversion from uint to string memory requested.
