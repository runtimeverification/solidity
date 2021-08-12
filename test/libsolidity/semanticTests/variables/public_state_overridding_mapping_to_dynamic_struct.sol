pragma abicoder               v2;

struct S { uint v; string s; }

contract A
{
	function test(uint x) external virtual returns (uint v, string memory s)
	{
	    v = x;
	    s = "test";
	}
}
contract X is A
{
	mapping(uint => S) public override test;

	function set() public { test[42].v = 2; test[42].s = "statevar"; }
}


// ----
// test(uint): 0 -> 0, "" 
// test(uint): 42 -> 0, ""
// set() ->
// test(uint): 0 -> 0, ""
// test(uint): 42 -> 2, "statevar"
