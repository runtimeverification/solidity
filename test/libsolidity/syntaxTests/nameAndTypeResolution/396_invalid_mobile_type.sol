    contract C {
        function f() public {
            // Invalid number
            78901234567890123456789012345678901234567890123456789345678901234567890012345678012345678901234567;
            [1, 78901234567890123456789012345678901234567890123456789345678901234567890012345678012345678901234567];
        }
    }
// ----
// Warning 6133: (89-187): Statement has no effect.
// Warning 6133: (201-304): Statement has no effect.
// Warning 2018: (25-315): Function state mutability can be restricted to pure
