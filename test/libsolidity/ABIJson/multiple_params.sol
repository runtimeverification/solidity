contract test {
    function f(uint256 a, uint256 b) public returns (uint256 d) { return a + b; }
}
// ----
//     :test
// [
//   {
//     "ieleName": "f(uint256,uint256)",
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
//     "name": "f",
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
