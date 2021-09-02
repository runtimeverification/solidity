contract test {
    function f(uint256 a) public returns (uint256 d) { return a * 7; }
    event e1(uint256 b, address indexed c);
    event e2();
    event e2(uint256 a);
    event e3() anonymous;
}
// ----
//     :test
// [
//   {
//     "anonymous": false,
//     "inputs":
//     [
//       {
//         "indexed": false,
//         "internalType": "uint256",
//         "name": "b",
//         "type": "uint256"
//       },
//       {
//         "indexed": true,
//         "internalType": "address",
//         "name": "c",
//         "type": "address"
//       }
//     ],
//     "name": "e1",
//     "type": "event"
//   },
//   {
//     "anonymous": false,
//     "inputs": [],
//     "name": "e2",
//     "type": "event"
//   },
//   {
//     "anonymous": false,
//     "inputs":
//     [
//       {
//         "indexed": false,
//         "internalType": "uint256",
//         "name": "a",
//         "type": "uint256"
//       }
//     ],
//     "name": "e2",
//     "type": "event"
//   },
//   {
//     "anonymous": true,
//     "inputs": [],
//     "name": "e3",
//     "type": "event"
//   },
//   {
//     "ieleName": "f(uint256)",
//     "inputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "a",
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
