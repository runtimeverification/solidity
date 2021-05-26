contract B{}

enum E { Zero }

contract C
{
    function f() public pure {

        uint16 a = uint16(int8(-1));

        int8 b = -1;
        uint16 c = uint16(b);

        int8 d = int8(uint16(type(uint16).max));

        uint16 e = type(uint16).max;
        int8 g = int8(e);

        address h = address(uint256(type(uint256).max));

        uint256 i = uint256(address(0));

        uint256 j = type(uint256).max;
        address k = address(j);

        int80 l = int80(bytes10("h"));
        bytes10 m = bytes10(int80(-1));

        B n = B(int256(100));
        int256 o = int256(new B());

        B p = B(0x00);

        int256 q = int256(E(0));
        int256 r = int256(E.Zero);
    }
}
// ----
// TypeError 9640: (95-111): Explicit type conversion not allowed from "int8" to "uint16".
// TypeError 9640: (154-163): Explicit type conversion not allowed from "int8" to "uint16".
// TypeError 9640: (183-213): Explicit type conversion not allowed from "uint16" to "int8".
// TypeError 9640: (270-277): Explicit type conversion not allowed from "uint16" to "int8".
// TypeError 9640: (300-335): Explicit type conversion not allowed from "uint256" to "address".
// TypeError 9640: (358-377): Explicit type conversion not allowed from "address" to "uint256".
// TypeError 9640: (439-449): Explicit type conversion not allowed from "uint256" to "address".
// TypeError 9640: (470-489): Explicit type conversion not allowed from "bytes10" to "int80".
// TypeError 9640: (511-529): Explicit type conversion not allowed from "int80" to "bytes10".
// TypeError 9640: (546-560): Explicit type conversion not allowed from "int256" to "contract B".
// TypeError 9640: (581-596): Explicit type conversion not allowed from "contract B" to "int256".
// TypeError 9640: (613-620): Explicit type conversion not allowed from "int_const 0" to "contract B".
// TypeError 9640: (642-654): Explicit type conversion not allowed from "enum E" to "int256".
// TypeError 9640: (675-689): Explicit type conversion not allowed from "enum E" to "int256".
