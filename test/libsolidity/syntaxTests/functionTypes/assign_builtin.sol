contract C {
  function f() public {
     function (uint256) view returns (bytes32) _blockhash = blockhash;
  }
}
// ----
// TypeError 9574: (42-106): Type function (uint256) view returns (bytes32) is not implicitly convertible to expected type function (uint256) view returns (bytes32). Special functions can not be converted to function types.
