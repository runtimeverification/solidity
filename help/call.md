# Error: `address.call` is not supported in IELE

*Note: the error message is different in the current compiler.*

In EVM-Solidity, a contract has a `call` member. It assembles a 
function name and arguments into an ABI-compliant bytestring. Here's
an example that passes three `uint256` integers to a `set`
function:

    bytes4 callee = bytes4(keccak256("set()"));
    
    ByteStoreContract store = new ByteStoreContract();
    store.call(callee, 1, 2, 3);

Because `call` is a security risk, IELE-Solidity doesn't support
it. 

You can emulate what `call` does by constructing a bytestring that's
byte-for-byte identical to the one `call` would make. Here's a first
attempt:

    bytes4 callee = bytes4(keccak256("set()")); -- same as above
    abi.encode(callee, 1, 2, 3)

The reason the result isn't identical is that, in IELE-Solidity, `uint` and `uint256`
are distinct types with different encodings. In EVM-Solidity,
constants like `1` are of type `uint256`. In IELE-Solidity, they're of
the non-overflowing `uint` type. To get a byte-for-byte identical
bytestring, you must convert the `uint` values to `uint256`:

    abi.encode(callee, uint256(1), uint256(2), uint256(3))

Having done that, you can send the bytestring by passing it as an
argument in the normal way:

    store.set(abi.encode(callee, uint256(1), uint256(2), uint256(3)));

The receiving function can't use `msg.data` to fetch the arguments,
because `msg.data` also doesn't exist in IELE-Solidity. Instead, the
data is received in an argument of type `bytes`:

    function set(bytes b) ...
    
When might you want to go through this process? Consider a trick used
by some contracts, such as
[this multi-signature wallet](https://github.com/ethereum/solidity/blob/v0.4.24/test/contracts/Wallet.cpp#L401).
The contents of `msg.data` are hashed and returned so that other
participants can use it to "cosign" the original function call.

The changes described above would allow the same trick, but that's not
the only way to return a hash for cosigning. See, for example,
[this IELE-compatible version](https://github.com/runtimeverification/solidity/blob/sol2iele/test/contracts/Wallet.cpp#L391)
of the same contract.

A related trick is to send arguments to the nominally
zero-argument fallback function. Here's an example from [another multi-signature wallet](https://github.com/BitGo/eth-multisig-v2/blob/ERC20/contracts/WalletSimple.sol#L66):

    function() payable {
      if (msg.value > 0) {
        // Fire deposited event if we are receiving funds
        Deposited(msg.sender, msg.value, msg.data);
      }
    }
    
... which is called like this:

    toAddress.call.value(value)(data)
    
In this case, there's no workaround. You'll have to use a named
function instead of the fallback function.


## Examples

* Before: [examples/call-and-msg-data.evm.sol](examples/call-and-msg-data.evm.sol)
* After: [examples/call-and-msg-data.iele.sol](examples/call-and-msg-data.iele.sol)

