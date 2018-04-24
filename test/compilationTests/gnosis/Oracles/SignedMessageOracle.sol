pragma solidity ^0.4.11;
import "../Oracles/Oracle.sol";


/// @title Signed message oracle contract - Allows to set an outcome with a signed message
/// @author Stefan George - <stefan@gnosis.pm>
contract SignedMessageOracle is Oracle {

    /*
     *  Events
     */
    event SignerReplacement(address indexed newSigner);
    event OutcomeAssignment(int256 outcome);

    /*
     *  Storage
     */
    address public signer;
    bytes32 public descriptionHash;
    uint256 nonce;
    bool public isSet;
    int256 public outcome;

    /*
     *  Modifiers
     */
    modifier isSigner () {
        // Only signer is allowed to proceed
        require(msg.sender == signer);
        _;
    }

    /*
     *  Public functions
     */
    /// @dev Constructor sets signer address based on signature
    /// @param _descriptionHash Hash identifying off chain event description
    /// @param v Signature parameter
    /// @param r Signature parameter
    /// @param s Signature parameter
    function SignedMessageOracle(bytes32 _descriptionHash, uint8 v, bytes32 r, bytes32 s)
        public
    {
        signer = ecrecover(_descriptionHash, v, r, s);
        descriptionHash = _descriptionHash;
    }

    /// @dev Replaces signer
    /// @param newSigner New signer
    /// @param _nonce Unique nonce to prevent replay attacks
    /// @param v Signature parameter
    /// @param r Signature parameter
    /// @param s Signature parameter
    function replaceSigner(address newSigner, uint256 _nonce, uint8 v, bytes32 r, bytes32 s)
        public
        isSigner
    {
        // Result is not set yet and nonce and signer are valid
        require(   !isSet
                && _nonce > nonce
                && signer == ecrecover(keccak256(descriptionHash, newSigner, _nonce), v, r, s));
        nonce = _nonce;
        signer = newSigner;
        SignerReplacement(newSigner);
    }

    /// @dev Sets outcome based on signed message
    /// @param _outcome Signed event outcome
    /// @param v Signature parameter
    /// @param r Signature parameter
    /// @param s Signature parameter
    function setOutcome(int256 _outcome, uint8 v, bytes32 r, bytes32 s)
        public
    {
        // Result is not set yet and signer is valid
        require(   !isSet
                && signer == ecrecover(keccak256(descriptionHash, _outcome), v, r, s));
        isSet = true;
        outcome = _outcome;
        OutcomeAssignment(_outcome);
    }

    /// @dev Returns if winning outcome
    /// @return Is outcome set?
    function isOutcomeSet()
        public
        constant
        returns (bool)
    {
        return isSet;
    }

    /// @dev Returns winning outcome
    /// @return Outcome
    function getOutcome()
        public
        constant
        returns (int256)
    {
        return outcome;
    }
}
