contract C {
  function f() public pure {
    abi.decode();
    abi.decode(msg.data);
    abi.decode(msg.data, uint, uint);
  }
}
// ----
// TypeError 5782: (46-58): This function takes two arguments, but 0 were provided.
// TypeError 2699: (75-83): msg.data is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 5782: (64-84): This function takes two arguments, but 1 were provided.
// TypeError 2699: (101-109): msg.data is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 5782: (90-122): This function takes two arguments, but 3 were provided.
// TypeError 6444: (111-115): The second argument to "abi.decode" has to be a tuple of types.
