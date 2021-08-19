contract Test {
    function assignment() public {
        uint8 uint8Min = type(int256).min;
        uint256 uintMin = type(int256).min;

        if (type(int256).min == 2**256 - 1) {
            uintMin;
        }

    }
}
// ----
// TypeError 9574: (59-92): Type int256 is not implicitly convertible to expected type uint8.
// TypeError 9574: (102-136): Type int256 is not implicitly convertible to expected type uint256.
// TypeError 2271: (151-181): Operator == not compatible with types int256 and int_const 1157...(70 digits omitted)...9935
