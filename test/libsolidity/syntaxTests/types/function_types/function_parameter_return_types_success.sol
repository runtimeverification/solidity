contract Test
{
  function uint_to_uint(uint x) internal pure returns (uint) { return x; }
  function uint_to_string(uint x) internal pure returns (string memory) { return x == 0 ? "a" : "b"; }
  function string_to_uint(string memory x) internal pure returns (uint) { return bytes(x).length; }
  function string_to_string(string memory x) internal pure returns (string memory) { return x; }

  function uint_uint_to_uint(uint x, uint y) internal pure returns (uint) { return x + y; }
  function uint_uint_to_string(uint x, uint y) internal pure returns (string memory) { return x == y ? "a" : "b"; }
  function string_uint_to_string(string memory x, uint y) internal pure returns (string memory) { return y == 0 ? "a" : x; }
  function string_string_to_string(string memory x, string memory y) internal pure returns (string memory) { return bytes(x).length == 0 ? y : x; }
  function uint_string_to_string(uint x, string memory y) internal pure returns (string memory) { return x == 0 ? "a" : y; }

  function tests() internal pure
  {
    function (uint) internal pure returns (uint) var_uint_to_uint = uint_to_uint;
    function (uint) internal pure returns (string memory) var_uint_to_string = uint_to_string;
    function (string memory) internal pure returns (uint) var_string_to_uint = string_to_uint;
    function (string memory) internal pure returns (string memory) var_string_to_string = string_to_string;

    function (uint, uint) internal pure returns (uint) var_uint_uint_to_uint = uint_uint_to_uint;
    function (uint, uint) internal pure returns (string memory) var_uint_uint_to_string = uint_uint_to_string;
    function (string memory, uint) internal pure returns (string memory) var_string_uint_to_string = string_uint_to_string;
    function (string memory, string memory) internal pure returns (string memory) var_string_string_to_string = string_string_to_string;
    function (uint, string memory) internal pure returns (string memory) var_uint_string_to_string = uint_string_to_string;

    // Avoid unused variable warnings:
    var_uint_to_uint(1);
    var_uint_to_string(2);
    var_string_to_uint("a");
    var_string_to_string("b");
    var_uint_uint_to_uint(3, 4);
    var_uint_uint_to_string(5, 6);
    var_string_uint_to_string("c", 7);
    var_string_string_to_string("d", "e");
    var_uint_string_to_string(8, "f");
  }
}
// ----
