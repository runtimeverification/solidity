contract c {
  bytes public b;
  function f() public {
    b = msg.data[:];
  }
}
// ----
// TypeError 2699: (63-71): msg.data is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
