// ┏━━━┓━┏┓━┏┓━━┏━━━┓━━┏━━━┓━━━━┏━━━┓━━━━━━━━━━━━━━━━━━━┏┓━━━━━┏━━━┓━━━━━━━━━┏┓━━━━━━━━━━━━━━┏┓━
// ┃┏━━┛┏┛┗┓┃┃━━┃┏━┓┃━━┃┏━┓┃━━━━┗┓┏┓┃━━━━━━━━━━━━━━━━━━┏┛┗┓━━━━┃┏━┓┃━━━━━━━━┏┛┗┓━━━━━━━━━━━━┏┛┗┓
// ┃┗━━┓┗┓┏┛┃┗━┓┗┛┏┛┃━━┃┃━┃┃━━━━━┃┃┃┃┏━━┓┏━━┓┏━━┓┏━━┓┏┓┗┓┏┛━━━━┃┃━┗┛┏━━┓┏━┓━┗┓┏┛┏━┓┏━━┓━┏━━┓┗┓┏┛
// ┃┏━━┛━┃┃━┃┏┓┃┏━┛┏┛━━┃┃━┃┃━━━━━┃┃┃┃┃┏┓┃┃┏┓┃┃┏┓┃┃━━┫┣┫━┃┃━━━━━┃┃━┏┓┃┏┓┃┃┏┓┓━┃┃━┃┏┛┗━┓┃━┃┏━┛━┃┃━
// ┃┗━━┓━┃┗┓┃┃┃┃┃┃┗━┓┏┓┃┗━┛┃━━━━┏┛┗┛┃┃┃━┫┃┗┛┃┃┗┛┃┣━━┃┃┃━┃┗┓━━━━┃┗━┛┃┃┗┛┃┃┃┃┃━┃┗┓┃┃━┃┗┛┗┓┃┗━┓━┃┗┓
// ┗━━━┛━┗━┛┗┛┗┛┗━━━┛┗┛┗━━━┛━━━━┗━━━┛┗━━┛┃┏━┛┗━━┛┗━━┛┗┛━┗━┛━━━━┗━━━┛┗━━┛┗┛┗┛━┗━┛┗┛━┗━━━┛┗━━┛━┗━┛
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┃┃━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┗┛━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// SPDX-License-Identifier: CC0-1.0

// This interface is designed to be compatible with the Vyper version.
/// @notice This is the Ethereum 2.0 deposit contract interface.
/// For more information see the Phase 0 specification under https://github.com/ethereum/eth2.0-specs
interface IDepositContract {
    /// @notice A processed deposit event.
    event DepositEvent(
        bytes pubkey,
        bytes withdrawal_credentials,
        bytes amount,
        bytes signature,
        bytes index
    );

    /// @notice Submit a Phase 0 DepositData object.
    /// @param pubkey A BLS12-381 public key.
    /// @param withdrawal_credentials Commitment to a public key for withdrawals.
    /// @param signature A BLS12-381 signature.
    /// @param deposit_data_root The SHA-256 hash of the SSZ-encoded DepositData object.
    /// Used as a protection against malformed input.
    function deposit(
        bytes calldata pubkey,
        bytes calldata withdrawal_credentials,
        bytes calldata signature,
        bytes32 deposit_data_root
    ) external payable;

    /// @notice Query the current deposit root hash.
    /// @return The deposit root hash.
    function get_deposit_root() external view returns (bytes32);

    /// @notice Query the current deposit count.
    /// @return The deposit count encoded as a little endian 64-bit number.
    function get_deposit_count() external view returns (bytes memory);
}

// Based on official specification in https://eips.ethereum.org/EIPS/eip-165
interface ERC165 {
    /// @notice Query if a contract implements an interface
    /// @param interfaceId The interface identifier, as specified in ERC-165
    /// @dev Interface identification is specified in ERC-165. This function
    ///  uses less than 30,000 gas.
    /// @return `true` if the contract implements `interfaceId` and
    ///  `interfaceId` is not 0xffffffff, `false` otherwise
    function supportsInterface(bytes4 interfaceId) external pure returns (bool);
}

// This is a rewrite of the Vyper Eth2.0 deposit contract in Solidity.
// It tries to stay as close as possible to the original source code.
/// @notice This is the Ethereum 2.0 deposit contract interface.
/// For more information see the Phase 0 specification under https://github.com/ethereum/eth2.0-specs
contract DepositContract is IDepositContract, ERC165 {
    uint constant DEPOSIT_CONTRACT_TREE_DEPTH = 32;
    // NOTE: this also ensures `deposit_count` will fit into 64-bits
    uint constant MAX_DEPOSIT_COUNT = 2**DEPOSIT_CONTRACT_TREE_DEPTH - 1;

    bytes32[DEPOSIT_CONTRACT_TREE_DEPTH] branch;
    uint256 deposit_count;

    bytes32[DEPOSIT_CONTRACT_TREE_DEPTH] zero_hashes;

    constructor() public {
        // Compute hashes in empty sparse Merkle tree
        for (uint height = 0; height < DEPOSIT_CONTRACT_TREE_DEPTH - 1; height++)
            zero_hashes[height + 1] = sha256(abi.encodePacked(zero_hashes[height], zero_hashes[height]));
    }

    function get_deposit_root() override external view returns (bytes32) {
        bytes32 node;
        uint size = deposit_count;
        for (uint height = 0; height < DEPOSIT_CONTRACT_TREE_DEPTH; height++) {
            if ((size & 1) == 1)
                node = sha256(abi.encodePacked(branch[height], node));
            else
                node = sha256(abi.encodePacked(node, zero_hashes[height]));
            size /= 2;
        }
        return sha256(abi.encodePacked(
            node,
            to_little_endian_64(uint64(deposit_count)),
            bytes24(0)
        ));
    }

    function get_deposit_count() override external view returns (bytes memory) {
        return to_little_endian_64(uint64(deposit_count));
    }

    function deposit(
        bytes calldata pubkey,
        bytes calldata withdrawal_credentials,
        bytes calldata signature,
        bytes32 deposit_data_root
    ) override external payable {
        // Extended ABI length checks since dynamic types are used.
        require(pubkey.length == 48, "DepositContract: invalid pubkey length");
        require(withdrawal_credentials.length == 32, "DepositContract: invalid withdrawal_credentials length");
        require(signature.length == 96, "DepositContract: invalid signature length");

        // Check deposit amount
        require(msg.value >= 1 ether, "DepositContract: deposit value too low");
        require(msg.value % 1 gwei == 0, "DepositContract: deposit value not multiple of gwei");
        uint deposit_amount = msg.value / 1 gwei;
        require(deposit_amount <= type(uint64).max, "DepositContract: deposit value too high");

        // Emit `DepositEvent` log
        bytes memory amount = to_little_endian_64(uint64(deposit_amount));
        emit DepositEvent(
            pubkey,
            withdrawal_credentials,
            amount,
            signature,
            to_little_endian_64(uint64(deposit_count))
        );

        // Compute deposit data root (`DepositData` hash tree root)
        bytes32 pubkey_root = sha256(abi.encodePacked(pubkey, bytes16(0)));
        bytes32 signature_root = sha256(abi.encodePacked(
            sha256(abi.encodePacked(signature[:64])),
            sha256(abi.encodePacked(signature[64:], bytes32(0)))
        ));
        bytes32 node = sha256(abi.encodePacked(
            sha256(abi.encodePacked(pubkey_root, withdrawal_credentials)),
            sha256(abi.encodePacked(amount, bytes24(0), signature_root))
        ));

        // Verify computed and expected deposit data roots match
        require(node == deposit_data_root, "DepositContract: reconstructed DepositData does not match supplied deposit_data_root");

        // Avoid overflowing the Merkle tree (and prevent edge case in computing `branch`)
        require(deposit_count < MAX_DEPOSIT_COUNT, "DepositContract: merkle tree full");

        // Add deposit data root to Merkle tree (update a single `branch` node)
        deposit_count += 1;
        uint size = deposit_count;
        for (uint height = 0; height < DEPOSIT_CONTRACT_TREE_DEPTH; height++) {
            if ((size & 1) == 1) {
                branch[height] = node;
                return;
            }
            node = sha256(abi.encodePacked(branch[height], node));
            size /= 2;
        }
        // As the loop should always end prematurely with the `return` statement,
        // this code should be unreachable. We assert `false` just to be safe.
        assert(false);
    }

    function supportsInterface(bytes4 interfaceId) override external pure returns (bool) {
        return interfaceId == type(ERC165).interfaceId || interfaceId == type(IDepositContract).interfaceId;
    }

    function to_little_endian_64(uint64 value) internal pure returns (bytes memory ret) {
        ret = new bytes(8);
        bytes8 bytesValue = bytes8(value);
        // Byteswapping during copying to bytes.
        ret[0] = bytesValue[7];
        ret[1] = bytesValue[6];
        ret[2] = bytesValue[5];
        ret[3] = bytesValue[4];
        ret[4] = bytesValue[3];
        ret[5] = bytesValue[2];
        ret[6] = bytesValue[1];
        ret[7] = bytesValue[0];
    }
}
// ====
// compileViaYul: also
// ----
// constructor()
// supportsInterface(bytes4): 0x0 -> 0
// supportsInterface(bytes4): 0x00ffffffff -> false # defined to be false by ERC-165 #
// supportsInterface(bytes4): 0x01ffc9a7 -> true # ERC-165 id #
// supportsInterface(bytes4): 0x0085640907 -> true # the deposit interface id #
// get_deposit_root() -> 0x00d70a234731285c6804c2a4f56711ddb8c82c99740f207854891028af34e27e5e
// get_deposit_count() -> "\x00\x00\x00\x00\x00\x00\x00\x00"
// # TODO: check balance and logs after each deposit #
// deposit(bytes,bytes,bytes,bytes32), 32 ether: 0 -> FAILURE, 2 # Empty input #
// get_deposit_root() -> 0x00d70a234731285c6804c2a4f56711ddb8c82c99740f207854891028af34e27e5e
// get_deposit_count() -> "\x00\x00\x00\x00\x00\x00\x00\x00"
// deposit(bytes,bytes,bytes,bytes32), 1 ether: "\x93\x3a\xd9\x49\x1b\x62\x05\x9d\xd0\x65\xb5\x60\xd2\x56\xd8\x95\x7a\x8c\x40\x2c\xc6\xe8\xd8\xee\x72\x90\xae\x11\xe8\xf7\x32\x92\x67\xa8\x81\x1c\x39\x75\x29\xda\xc5\x2a\xe1\x34\x2b\xa5\x8c\x95", "\x00\xf5\x04\x28\x67\x7c\x60\xf9\x97\xaa\xde\xab\x24\xaa\xbf\x7f\xce\xae\xf4\x91\xc9\x6a\x52\xb4\x63\xae\x91\xf9\x56\x11\xcf\x71", "\xa2\x9d\x01\xcc\x8c\x62\x96\xa8\x15\x0e\x51\x5b\x59\x95\x39\x0e\xf8\x41\xdc\x18\x94\x8a\xa3\xe7\x9b\xe6\xd7\xc1\x85\x1b\x4c\xbb\x5d\x6f\xf4\x9f\xa7\x0b\x9c\x78\x23\x99\x50\x6a\x22\xa8\x51\x93\x15\x1b\x9b\x69\x12\x45\xce\xba\xfd\x20\x63\x01\x24\x43\xc1\x32\x4b\x6c\x36\xde\xba\xed\xef\xb7\xb2\xd7\x1b\x05\x03\xff\xdc\x00\x15\x0a\xaf\xfd\x42\xe6\x33\x58\x23\x8e\xc8\x88\x90\x17\x38\xb8", 0x00aa4a8d0b7d9077248630f1a4701ae9764e42271d7f22b7838778411857fd349e -> # txhash: 0x7085c586686d666e8bb6e9477a0f0b09565b2060a11f1c4209d3a52295033832 #
// get_deposit_root() -> 0x2089653123d9c721215120b6db6738ba273bbc5228ac093b1f983badcdc8a438
// get_deposit_count() -> "\x01\x00\x00\x00\x00\x00\x00\x00"
// deposit(bytes,bytes,bytes,bytes32), 32 ether: "\xb2\xce\x0f\x79\xf9\x0e\x7b\x3a\x11\x3c\xa5\x78\x3c\x65\x75\x6f\x96\xc4\xb4\x67\x3c\x2b\x5c\x1e\xb4\xef\xc2\x22\x80\x25\x94\x41\x06\xd6\x01\x21\x1e\x88\x66\xdc\x5b\x50\xdc\x48\xa2\x44\xdd\x7c", "\x00\x34\x4b\x6c\x73\xf7\x1b\x11\xc5\x6a\xba\x0d\x01\xb7\xd8\xad\x83\x55\x9f\x20\x9d\x0a\x41\x01\xa5\x15\xf6\xad\x54\xc8\x97\x71", "\x94\x5c\xaa\xf8\x2d\x18\xe7\x8c\x03\x39\x27\xd5\x1f\x45\x2e\xbc\xd7\x65\x24\x49\x7b\x91\xd7\xa1\x12\x19\xcb\x3d\xb6\xa1\xd3\x69\x75\x95\xfc\x09\x5c\xe4\x89\xe4\x6b\x2e\xf1\x29\x59\x1f\x2f\x6d\x07\x9b\xe4\xfa\xaf\x34\x5a\x02\xc5\xeb\x13\x3c\x07\x2e\x7c\x56\x0c\x6c\x36\x17\xee\xe6\x6b\x4b\x87\x81\x65\xc5\x02\x35\x7d\x49\x48\x53\x26\xbc\x6b\x31\xbc\x96\x87\x3f\x30\x8c\x8f\x19\xc0\x9d", 0x00dbd986dc85ceb382708cf90a3500f500f0a393c5ece76963ac3ed72eccd2c301 -> # txhash: 0x404d8e109822ce448e68f45216c12cb051b784d068fbe98317ab8e50c58304ac #
// get_deposit_root() -> 0x40255975859377d912c53aa853245ebd939bdd2b33a28e084babdcc1ed8238ee
// get_deposit_count() -> "\x02\x00\x00\x00\x00\x00\x00\x00"
