contract test {
    function f(uint256, uint256 k) public returns (uint256 ret_k, uint256 ret_g) {
        uint256 g = 8;
        ret_k = k;
        ret_g = g;
    }
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
//         "name": "",
//         "type": "uint256"
//       },
//       {
//         "internalType": "uint256",
//         "name": "k",
//         "type": "uint256"
//       }
//     ],
//     "name": "f",
//     "outputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "ret_k",
//         "type": "uint256"
//       },
//       {
//         "internalType": "uint256",
//         "name": "ret_g",
//         "type": "uint256"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
