contract test {
    constructor(uint256 param1, test param2, bool param3) payable {}
}
// ----
//     :test
// [
//   {
//     "inputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "param1",
//         "type": "uint256"
//       },
//       {
//         "internalType": "contract test",
//         "name": "param2",
//         "type": "address"
//       },
//       {
//         "internalType": "bool",
//         "name": "param3",
//         "type": "bool"
//       }
//     ],
//     "stateMutability": "payable",
//     "type": "constructor"
//   }
// ]
