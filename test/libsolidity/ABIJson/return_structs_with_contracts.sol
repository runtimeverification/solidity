pragma abicoder               v2;
contract C {
    struct S { C[] x; C y; }
    function f() public returns (S memory s, C c) {
    }
}
// ----
//     :C
// [
//   {
//     "ieleName": "f()",
//     "inputs": [],
//     "name": "f",
//     "outputs":
//     [
//       {
//         "components":
//         [
//           {
//             "internalType": "contract C[]",
//             "name": "x",
//             "type": "address[]"
//           },
//           {
//             "internalType": "contract C",
//             "name": "y",
//             "type": "address"
//           }
//         ],
//         "internalType": "struct C.S",
//         "name": "s",
//         "type": "tuple"
//       },
//       {
//         "internalType": "contract C",
//         "name": "c",
//         "type": "address"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
