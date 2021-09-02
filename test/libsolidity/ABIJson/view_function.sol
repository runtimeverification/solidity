contract test {
    function foo(uint256 a, uint256 b) public returns (uint256 d) { return a + b; }
    function boo(uint32 a) public view returns(uint256 b) { return a * 4; }
}
// ----
//     :test
// [
//   {
//     "ieleName": "boo(uint32)",
//     "inputs":
//     [
//       {
//         "internalType": "uint32",
//         "name": "a",
//         "type": "uint32"
//       }
//     ],
//     "name": "boo",
//     "outputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "b",
//         "type": "uint256"
//       }
//     ],
//     "stateMutability": "view",
//     "type": "function"
//   },
//   {
//     "ieleName": "foo(uint256,uint256)",
//     "inputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "a",
//         "type": "uint256"
//       },
//       {
//         "internalType": "uint256",
//         "name": "b",
//         "type": "uint256"
//       }
//     ],
//     "name": "foo",
//     "outputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "d",
//         "type": "uint256"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
