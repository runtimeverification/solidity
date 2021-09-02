contract Base {
    function baseFunction(uint256 p) public returns (uint256 i) { return p; }
    event baseEvent(bytes32 indexed evtArgBase);
}
contract Derived is Base {
    function derivedFunction(bytes32 p) public returns (bytes32 i) { return p; }
    event derivedEvent(uint256 indexed evtArgDerived);
}
// ----
//     :Base
// [
//   {
//     "anonymous": false,
//     "inputs":
//     [
//       {
//         "indexed": true,
//         "internalType": "bytes32",
//         "name": "evtArgBase",
//         "type": "bytes32"
//       }
//     ],
//     "name": "baseEvent",
//     "type": "event"
//   },
//   {
//     "ieleName": "baseFunction(uint256)",
//     "inputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "p",
//         "type": "uint256"
//       }
//     ],
//     "name": "baseFunction",
//     "outputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "i",
//         "type": "uint256"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
//
//
//     :Derived
// [
//   {
//     "anonymous": false,
//     "inputs":
//     [
//       {
//         "indexed": true,
//         "internalType": "bytes32",
//         "name": "evtArgBase",
//         "type": "bytes32"
//       }
//     ],
//     "name": "baseEvent",
//     "type": "event"
//   },
//   {
//     "anonymous": false,
//     "inputs":
//     [
//       {
//         "indexed": true,
//         "internalType": "uint256",
//         "name": "evtArgDerived",
//         "type": "uint256"
//       }
//     ],
//     "name": "derivedEvent",
//     "type": "event"
//   },
//   {
//     "ieleName": "baseFunction(uint256)",
//     "inputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "p",
//         "type": "uint256"
//       }
//     ],
//     "name": "baseFunction",
//     "outputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "i",
//         "type": "uint256"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   },
//   {
//     "ieleName": "derivedFunction(bytes32)",
//     "inputs":
//     [
//       {
//         "internalType": "bytes32",
//         "name": "p",
//         "type": "bytes32"
//       }
//     ],
//     "name": "derivedFunction",
//     "outputs":
//     [
//       {
//         "internalType": "bytes32",
//         "name": "i",
//         "type": "bytes32"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
