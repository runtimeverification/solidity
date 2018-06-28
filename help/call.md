# Error: Low-level calls are not supported in IELE

## Quick look

Old: 
    
    bytes4 callee = bytes4(keccak256("set()"));
    
    ByteStoreContract store = new ByteStoreContract();
    store.call(callee, 1, 2, 3);     // <<<<== change this
    
New:
    
    store.set(abi.encode(callee, uint256(1), uint256(2), uint256(3)));
   
If the called function is also being compiled, you'll likely need to 
[fix its use of `msg.data`](./msg-data.md).
   
## Discussion

In EVM-Solidity, a contract has a `call` member that can be used
to pass an arbitrary byte string. Here's an example that passes three
encoded `uint256` integers to a `set` function:

    bytes4 callee = bytes4(keccak256("set()"));
    
    ByteStoreContract store = new ByteStoreContract();
    store.call(callee, 1, 2, 3);

Because `call` is a security risk, IELE-Solidity doesn't support
it. Instead, `set` is called just like any other public function:

    store.set(...);
    
    
`store.set` takes a single argument (of type `bytes`) that must
conform to the ABI.

The target function's name is encoded the same way as in EVM-Solidity:

    bytes4 callee = bytes4(keccak256("set()"));

However, unlike `call`, the byte data isn't assembled from multiple
arguments. You have to assemble it yourself, using
`abi.encode`. The following is a correct use of that function:

    abi.encode(callee, 1, 2, 3)

There's a potential problem, though. In EVM-Solidity, constants like
`1` are of type `uint256`. In IELE-Solidity, they're of the
non-overflowing `uint` type. Because they're no longer the same type,
IELE-Solidity must encode `uint` arguments differently than `uint256`.
In the above example, they'd be encoded differently than an
EVM-Solidity compiler would.

You might not care. Perhaps you haven't written `ByteStoreContract` yet, so
you're fine with arbitrarily large integers. 
If you're working with a pre-existing contract, though, you'd probably
be wise to send exactly the same payload to it as EVM-Solidity would. So 
cast the `uint` constants to `uint256` before encoding:

    abi.encode(callee, uint256(1), uint256(2), uint256(3))
