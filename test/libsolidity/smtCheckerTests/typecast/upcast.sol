pragma experimental SMTChecker;

abstract contract D {}

contract C {
	function f1() public pure {
		// unsigned <- signed
		uint16 x = type(uint16).max;
		assert(x == 65535);
		int8 i = int8(-1);
		assert(i == -1);
		x = uint16(uint8(int8(-1)));
		assert(x == 255);
		x = uint16(int16(i));
		assert(x == 65535);
		uint256 z = uint256(uint8(i));
		assert(z == 255);
	}

	function f2() public pure {
		// signed <- unsigned
		int16 y = int16(uint16(uint8(uint256(65535))));
		assert(y == 255);
		int256 z = int256(uint256(uint8(type(uint256).max)));
		assert(z == 255);
		z = int256(uint256(uint8(255)));
		assert(z == 255);
	}

	function f3() public pure {
		// signed <- signed
		int16 y = int16(uint16(uint8(int8(int256(uint256(65535))))));
		assert(y == 255);
		int256 z = int256(int8(-1));
		assert(z == -1);
		z = int256(int8(int256(255)));
		assert(z == -1);
		z = int256(int16(5000));
		assert(z == 5000);
	}

	function f4() public pure {
		// unsigned <- unsigned
		uint256 x = uint256(uint8(type(uint256).max));
		assert(x == 255);
		x = uint256(uint16(type(uint256).max));
		assert(x == 65535);
		x = uint256(uint16(5000));
		assert(x == 5000);
		uint16 y = uint16(type(uint256).max);
		assert(y == 65535);
		y = uint16(uint8(type(uint16).max));
		assert(y == 255);
		address a = address(uint160(uint8(0)));
		assert(a == address(0));
		D d = D(address(uint160(uint8(0))));
		assert(a == address(d));
		bytes2 b1 = 0xcafe;
		bytes4 b2 = bytes4(b1);
		assert(b2 == 0xcafe0000);
		bytes16 b3 = bytes16(b1);
		assert(b3 == 0xcafe0000000000000000000000000000);
		bytes4 b4 = bytes4(uint32(0xcafe));
		assert(b4 == 0x0000cafe);
		bytes4 b5 = bytes4(uint32(0xcafe0000));
		assert(b5 == 0xcafe0000);
	}
}
// ----
