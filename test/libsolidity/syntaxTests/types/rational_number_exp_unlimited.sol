contract c {
    function f() public pure {
        int a;
        a = 4 ** 4 ** 2 ** 4 ** 4 ** 4 ** 4;
        a = -4 ** 4 ** 2 ** 4 ** 4 ** 4 ** 4 ** 4;
        a = 4 ** (-(2 ** 4 ** 4 ** 4 ** 4 ** 4));
        a = 0 ** 1E1233; // fine
        a = 1 ** 1E1233; // fine
        a = -1 ** 1E1233; // fine
        a = 2 ** 1E1233;
        a = -2 ** 1E1233;
        a = 2 ** -1E1233;
        a = -2 ** -1E1233;
        a = 1E1233 ** 2;
        a = -1E1233 ** 2;
        a = 1E1233 ** -2;
        a = -1E1233 ** -2;
        a = 1E1233 ** 1E1233;
        a = 1E1233 ** -1E1233;
        a = -1E1233 ** 1E1233;
        a = -1E1233 ** -1E1233;
    }
}
// ----
// TypeError 2271: (86-102): Operator ** not compatible with types int_const 4 and int_const 1340...(147 digits omitted)...4096
// TypeError 2271: (71-102): Operator ** not compatible with types int_const 4 and int_const 4294967296
// TypeError 2271: (137-153): Operator ** not compatible with types int_const 4 and int_const 1340...(147 digits omitted)...4096
// TypeError 2271: (122-153): Operator ** not compatible with types int_const 4 and int_const 1157...(70 digits omitted)...9936
// TypeError 2271: (185-201): Operator ** not compatible with types int_const 4 and int_const 1340...(147 digits omitted)...4096
// TypeError 2271: (167-203): Operator ** not compatible with types int_const 4 and int_const -115...(71 digits omitted)...9936
// TypeError 2271: (317-328): Operator ** not compatible with types int_const 2 and int_const 1000...(1226 digits omitted)...0000
// TypeError 2271: (342-354): Operator ** not compatible with types int_const -2 and int_const 1000...(1226 digits omitted)...0000
// TypeError 2271: (368-380): Operator ** not compatible with types int_const 2 and int_const -100...(1227 digits omitted)...0000
// TypeError 2271: (394-407): Operator ** not compatible with types int_const -2 and int_const -100...(1227 digits omitted)...0000
// TypeError 2271: (472-484): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const -2. Precision of rational constants is limited to 4096 bits.
// TypeError 2271: (498-511): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const -2. Precision of rational constants is limited to 4096 bits.
// TypeError 2271: (525-541): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError 2271: (555-572): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
// TypeError 2271: (586-603): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError 2271: (617-635): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000

