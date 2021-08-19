abstract contract Test
{
    function uint_to_uint(uint x) internal pure returns (uint) { return x; }
    function uint_to_string(uint x) internal pure returns (string memory) { return x == 0 ? "a" : "b"; }
    function uint_to_string_storage(uint) internal pure returns (string storage) {}
    function string_to_uint(string memory x) internal pure returns (uint) { return bytes(x).length; }
    function string_to_string(string memory x) internal pure returns (string memory) { return x; }

    function uint_uint_to_uint(uint x, uint y) internal pure returns (uint) { return x + y; }
    function uint_uint_to_string(uint x, uint y) internal pure returns (string memory) { return x == y ? "a" : "b"; }
    function string_uint_to_string(string memory x, uint y) internal pure returns (string memory) { return y == 0 ? "a" : x; }
    function string_string_to_string(string memory x, string memory y) internal pure returns (string memory) { return bytes(x).length == 0 ? y : x; }
    function uint_string_to_string(uint x, string memory y) internal pure returns (string memory) { return x == 0 ? "a" : y; }

    function tests() internal pure
    {
      function (uint) internal pure returns (uint) var_uint_to_uint = uint_to_string;
      function (uint) internal pure returns (string memory) var_uint_to_string = uint_to_string_storage;
      function (string memory) internal pure returns (uint) var_string_to_uint = uint_to_string;
      function (string memory) internal pure returns (string memory) var_string_to_string = var_uint_to_string;

      function (uint, uint) internal pure returns (uint) var_uint_uint_to_uint = uint_to_uint;
      function (string memory, uint) internal pure returns (string memory) var_string_uint_to_string = string_to_string;
      function (string memory, string memory) internal pure returns (string memory) var_string_string_to_string = string_to_string;

      var_uint_to_uint(1);
      var_uint_to_string(2);
      var_string_to_uint("a");
      var_string_to_string("b");
      var_uint_uint_to_uint(3, 4);
      var_string_uint_to_string("c", 7);
      var_string_string_to_string("d", "e");
    }
}
// ----
// TypeError 9574: (1157-1235): Type function (uint) pure returns (string memory) is not implicitly convertible to expected type function (uint) pure returns (uint).
// TypeError 9574: (1243-1340): Type function (uint) pure returns (string storage pointer) is not implicitly convertible to expected type function (uint) pure returns (string memory).
// TypeError 9574: (1348-1437): Type function (uint) pure returns (string memory) is not implicitly convertible to expected type function (string memory) pure returns (uint).
// TypeError 9574: (1445-1549): Type function (uint) pure returns (string memory) is not implicitly convertible to expected type function (string memory) pure returns (string memory).
// TypeError 9574: (1558-1645): Type function (uint) pure returns (uint) is not implicitly convertible to expected type function (uint,uint) pure returns (uint).
// TypeError 9574: (1653-1766): Type function (string memory) pure returns (string memory) is not implicitly convertible to expected type function (string memory,uint) pure returns (string memory).
// TypeError 9574: (1774-1898): Type function (string memory) pure returns (string memory) is not implicitly convertible to expected type function (string memory,string memory) pure returns (string memory).
