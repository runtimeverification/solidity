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
// TypeError: (167-203): Operator ** not compatible with types int_const 4 and int_const -179...(302 digits omitted)...7216
// TypeError: (317-328): Operator ** not compatible with types int_const 2 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (342-354): Operator ** not compatible with types int_const -2 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (368-380): Operator ** not compatible with types int_const 2 and int_const -100...(1227 digits omitted)...0000
// TypeError: (394-407): Operator ** not compatible with types int_const -2 and int_const -100...(1227 digits omitted)...0000
// TypeError: (525-541): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (555-572): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
// TypeError: (586-603): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (617-635): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
