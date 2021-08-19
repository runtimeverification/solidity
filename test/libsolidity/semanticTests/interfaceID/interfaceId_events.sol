interface HelloWorld {
    function hello() external pure;
    function world(int256) external pure;
}

interface HelloWorldWithEvent {
    event Event();
    function hello() external pure;
    function world(int256) external pure;
}

contract Test {
    bytes4 public hello_world = type(HelloWorld).interfaceId;
    bytes4 public hello_world_with_event = type(HelloWorldWithEvent).interfaceId;
}

// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// hello_world() -> 0x00c6be8b58
// hello_world_with_event() -> 0x00c6be8b58
