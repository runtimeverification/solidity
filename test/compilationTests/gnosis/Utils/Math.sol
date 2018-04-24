pragma solidity ^0.4.11;


/// @title Math library - Allows calculation of logarithmic and exponential functions
/// @author Alan Lu - <alan.lu@gnosis.pm>
/// @author Stefan George - <stefan@gnosis.pm>
library Math {

    /*
     *  Constants
     */
    // This is equal to 1 in our calculations
    uint256 public constant ONE =  0x10000000000000000;
    uint256 public constant LN2 = 0xb17217f7d1cf79ac;
    uint256 public constant LOG2_E = 0x171547652b82fe177;

    /*
     *  Public functions
     */
    /// @dev Returns natural exponential function value of given x
    /// @param x x
    /// @return e**x
    function exp(int256 x)
        public
        constant
        returns (uint256)
    {
        // revert if x is > MAX_POWER, where
        // MAX_POWER = int256(mp.floor(mp.log(mpf(2**256 - 1) / ONE) * ONE))
        require(x <= 2454971259878909886679);
        // return 0 if exp(x) is tiny, using
        // MIN_POWER = int256(mp.floor(mp.log(mpf(1) / ONE) * ONE))
        if (x < -818323753292969962227)
            return 0;
        // Transform so that e^x -> 2^x
        x = x * int256(ONE) / int256(LN2);
        // 2^x = 2^whole(x) * 2^frac(x)
        //       ^^^^^^^^^^ is a bit shift
        // so Taylor expand on z = frac(x)
        int256 shift;
        uint256 z;
        if (x >= 0) {
            shift = x / int256(ONE);
            z = uint256(x % int256(ONE));
        }
        else {
            shift = x / int256(ONE) - 1;
            z = ONE - uint256(-x % int256(ONE));
        }
        // 2^x = 1 + (ln 2) x + (ln 2)^2/2! x^2 + ...
        //
        // Can generate the z coefficients using mpmath and the following lines
        // >>> from mpmath import mp
        // >>> mp.dps = 100
        // >>> ONE =  0x10000000000000000
        // >>> print('\n'.join(hex(int256(mp.log(2)**i / mp.factorial(i) * ONE)) for i in range(1, 7)))
        // 0xb17217f7d1cf79ab
        // 0x3d7f7bff058b1d50
        // 0xe35846b82505fc5
        // 0x276556df749cee5
        // 0x5761ff9e299cc4
        // 0xa184897c363c3
        uint256 zpow = z;
        uint256 result = ONE;
        result += 0xb17217f7d1cf79ab * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x3d7f7bff058b1d50 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0xe35846b82505fc5 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x276556df749cee5 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x5761ff9e299cc4 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0xa184897c363c3 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0xffe5fe2c4586 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x162c0223a5c8 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x1b5253d395e * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x1e4cf5158b * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x1e8cac735 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x1c3bd650 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x1816193 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x131496 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0xe1b7 * zpow / ONE;
        zpow = zpow * z / ONE;
        result += 0x9c7 * zpow / ONE;
        if (shift >= 0) {
            if (result >> (256-shift) > 0)
                return (2**256-1);
            return result << shift;
        }
        else
            return result >> (-shift);
    }

    /// @dev Returns natural logarithm value of given x
    /// @param x x
    /// @return ln(x)
    function ln(uint256 x)
        public
        constant
        returns (int256)
    {
        require(x > 0);
        // binary search for floor(log2(x))
        int256 ilog2 = floorLog2(x);
        int256 z;
        if (ilog2 < 0)
            z = int256(x << uint256(-ilog2));
        else
            z = int256(x >> uint256(ilog2));
        // z = x * 2^-⌊log₂x⌋
        // so 1 <= z < 2
        // and ln z = ln x - ⌊log₂x⌋/log₂e
        // so just compute ln z using artanh series
        // and calculate ln x from that
        int256 term = (z - int256(ONE)) * int256(ONE) / (z + int256(ONE));
        int256 halflnz = term;
        int256 termpow = term * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 3;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 5;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 7;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 9;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 11;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 13;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 15;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 17;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 19;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 21;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 23;
        termpow = termpow * term / int256(ONE) * term / int256(ONE);
        halflnz += termpow / 25;
        return (ilog2 * int256(ONE)) * int256(ONE) / int256(LOG2_E) + 2 * halflnz;
    }

    /// @dev Returns base 2 logarithm value of given x
    /// @param x x
    /// @return logarithmic value
    function floorLog2(uint256 x)
        public
        constant
        returns (int256 lo)
    {
        lo = -64;
        int256 hi = 193;
        // I use a shift here instead of / 2 because it floors instead of rounding towards 0
        int256 mid = (hi + lo) >> 1;
        while((lo + 1) < hi) {
            if (mid < 0 && x << uint256(-mid) < ONE || mid >= 0 && x >> uint256(mid) < ONE)
                hi = mid;
            else
                lo = mid;
            mid = (hi + lo) >> 1;
        }
    }

    /// @dev Returns maximum of an array
    /// @param nums Numbers to look through
    /// @return Maximum number
    function max(int256[] nums)
        public
        constant
        returns (int256 max)
    {
        require(nums.length > 0);
        max = -2**255;
        for (uint256 i = 0; i < nums.length; i++)
            if (nums[i] > max)
                max = nums[i];
    }

    /// @dev Returns whether an add operation causes an overflow
    /// @param a First addend
    /// @param b Second addend
    /// @return Did no overflow occur?
    function safeToAdd(uint256 a, uint256 b)
        public
        constant
        returns (bool)
    {
        return a + b >= a;
    }

    /// @dev Returns whether a subtraction operation causes an underflow
    /// @param a Minuend
    /// @param b Subtrahend
    /// @return Did no underflow occur?
    function safeToSub(uint256 a, uint256 b)
        public
        constant
        returns (bool)
    {
        return a >= b;
    }

    /// @dev Returns whether a multiply operation causes an overflow
    /// @param a First factor
    /// @param b Second factor
    /// @return Did no overflow occur?
    function safeToMul(uint256 a, uint256 b)
        public
        constant
        returns (bool)
    {
        return b == 0 || a * b / b == a;
    }

    /// @dev Returns sum if no overflow occurred
    /// @param a First addend
    /// @param b Second addend
    /// @return Sum
    function add(uint256 a, uint256 b)
        public
        constant
        returns (uint256)
    {
        require(safeToAdd(a, b));
        return a + b;
    }

    /// @dev Returns difference if no overflow occurred
    /// @param a Minuend
    /// @param b Subtrahend
    /// @return Difference
    function sub(uint256 a, uint256 b)
        public
        constant
        returns (uint256)
    {
        require(safeToSub(a, b));
        return a - b;
    }

    /// @dev Returns product if no overflow occurred
    /// @param a First factor
    /// @param b Second factor
    /// @return Product
    function mul(uint256 a, uint256 b)
        public
        constant
        returns (uint256)
    {
        require(safeToMul(a, b));
        return a * b;
    }

    /// @dev Returns whether an add operation causes an overflow
    /// @param a First addend
    /// @param b Second addend
    /// @return Did no overflow occur?
    function safeToAdd(int256 a, int256 b)
        public
        constant
        returns (bool)
    {
        return (b >= 0 && a + b >= a) || (b < 0 && a + b < a);
    }

    /// @dev Returns whether a subtraction operation causes an underflow
    /// @param a Minuend
    /// @param b Subtrahend
    /// @return Did no underflow occur?
    function safeToSub(int256 a, int256 b)
        public
        constant
        returns (bool)
    {
        return (b >= 0 && a - b <= a) || (b < 0 && a - b > a);
    }

    /// @dev Returns whether a multiply operation causes an overflow
    /// @param a First factor
    /// @param b Second factor
    /// @return Did no overflow occur?
    function safeToMul(int256 a, int256 b)
        public
        constant
        returns (bool)
    {
        return (b == 0) || (a * b / b == a);
    }

    /// @dev Returns sum if no overflow occurred
    /// @param a First addend
    /// @param b Second addend
    /// @return Sum
    function add(int256 a, int256 b)
        public
        constant
        returns (int256)
    {
        require(safeToAdd(a, b));
        return a + b;
    }

    /// @dev Returns difference if no overflow occurred
    /// @param a Minuend
    /// @param b Subtrahend
    /// @return Difference
    function sub(int256 a, int256 b)
        public
        constant
        returns (int256)
    {
        require(safeToSub(a, b));
        return a - b;
    }

    /// @dev Returns product if no overflow occurred
    /// @param a First factor
    /// @param b Second factor
    /// @return Product
    function mul(int256 a, int256 b)
        public
        constant
        returns (int256)
    {
        require(safeToMul(a, b));
        return a * b;
    }
}
