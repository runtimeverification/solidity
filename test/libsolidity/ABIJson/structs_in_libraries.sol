pragma abicoder               v2;
library L {
    struct S { uint256 a; T[] sub; bytes b; }
    struct T { uint256[2] x; }
    function f(L.S storage s) public view {}
    function g(L.S memory s) public view {}
}
// ----
//     :L
// [
//   {
//     "ieleName": "g(L.S)",
//     "inputs":
//     [
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
//             "internalType": "struct L.T[]",
//             "name": "sub",
//             "type": "tuple[]"
//           },
//           {
//             "internalType": "bytes",
//             "name": "b",
//             "type": "bytes"
//           }
//         ],
//         "internalType": "struct L.S",
//         "name": "s",
//         "type": "tuple"
//       }
//     ],
//     "name": "g",
//     "outputs": [],
//     "stateMutability": "view",
//     "type": "function"
//   }
// ]
