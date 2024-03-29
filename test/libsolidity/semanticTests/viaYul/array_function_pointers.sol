contract C {
	function f(uint n, uint m) public {
		function() internal returns (uint)[] memory arr = new function() internal returns (uint)[](n);
		arr[m]();
	}
	function f2(uint n, uint m, uint a, uint b) public {
		function() internal returns (uint)[][] memory arr = new function() internal returns (uint)[][](n);
		for (uint i = 0; i < n; ++i)
			arr[i] = new function() internal returns (uint)[](m);
		arr[a][b]();
	}
	function g(uint n, uint m) public {
		function() external returns (uint)[] memory arr = new function() external returns (uint)[](n);
		arr[m]();
	}
	function g2(uint n, uint m, uint a, uint b) public {
		function() external returns (uint)[][] memory arr = new function() external returns (uint)[][](n);
		for (uint i = 0; i < n; ++i)
			arr[i] = new function() external returns (uint)[](m);
		arr[a][b]();
	}
}
// ====
// compileViaYul: also
// ----
// f(uint,uint): 1823621, 12323 -> FAILURE, 1
// f2(uint,uint,uint,uint): 1872, 1823, 123, 1232 -> FAILURE, 1
// g(uint,uint): 1823621, 12323 -> FAILURE, 1
// g2(uint,uint,uint,uint): 1872, 1823, 123, 1232 -> FAILURE, 1
