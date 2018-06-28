# Error: msg.data is not supported in IELE.

## Quick look

Old: 
    
    contract ByteStoreContract {
      bytes data;
      
      function set() public {  // <<-- change
        data = msg.data;       // <<-- change
        ...
    
New:
    
      function set(bytes b) public {
        data = b;
        ...
        
If the calling function is also being compiled, you'll likely need to 
[fix its use of `call`](./low-level-calls.md).
   
## Discussion

Rather than extracting `bytes` data from the global `msg` object, it's
provided in an ordinary function argument of type `bytes`:

      function set(bytes b) public {
                   ^^^^^^^
                   
The local variable can now replace uses of `msg.data`. For example, if
the data is to be copied into blockchain storage under the name
`data`, that would be done like this:

        data = b;

Note: if `msg.data` is used in a function called from the `public`
function, you'll need to pass it explicitly:


Old: 
    
    bytes data;
    
    function set() public 
      helper()
      ...
     
    function helper() internal 
      data = msg.data
      
New:
    
    bytes data;
    
    function set(bytes b) public 
      helper(b)
      ...
     
    function helper(bytes b) internal 
      data = msg.data
      
## Examples

* Before: [examples/call-and-msg-data.evm.sol](examples/call-and-msg-data.evm.sol)
* After: [examples/call-and-msg-data.iele.sol](examples/call-and-msg-data.iele.sol)
