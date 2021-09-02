pragma abicoder               v2;
contract C {
    struct S { uint256 a; T[] sub; }
    struct T { uint256[2] x; }
    function f() public returns (uint256 x, S memory s) {
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
//         "internalType": "uint256",
//         "name": "x",
//         "type": "uint256"
//       },
//       {
//         "components":
//         [
//           {
//             "internalType": "uint256",
//             "name": "a",
//             "type": "uint256"
//           },
//           {
//             "components":
//             [
//               {
//                 "internalType": "uint256[2]",
//                 "name": "x",
//                 "type": "uint256[2]"
//               }
//             ],
//             "internalType": "struct C.T[]",
//             "name": "sub",
//             "type": "tuple[]"
//           }
//         ],
//         "internalType": "struct C.S",
//         "name": "s",
//         "type": "tuple"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
