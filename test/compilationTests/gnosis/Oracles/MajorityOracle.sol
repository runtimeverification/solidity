pragma solidity ^0.4.11;
import "../Oracles/Oracle.sol";


/// @title Majority oracle contract - Allows to resolve an event based on multiple oracles with majority vote
/// @author Stefan George - <stefan@gnosis.pm>
contract MajorityOracle is Oracle {

    /*
     *  Storage
     */
    Oracle[] public oracles;

    /*
     *  Public functions
     */
    /// @dev Allows to create an oracle for a majority vote based on other oracles
    /// @param _oracles List of oracles taking part in the majority vote
    function MajorityOracle(Oracle[] _oracles)
        public
    {
        // At least 2 oracles should be defined
        require(_oracles.length > 2);
        for (uint256 i = 0; i < _oracles.length; i++)
            // Oracle address cannot be null
            require(address(_oracles[i]) != 0);
        oracles = _oracles;
    }

    /// @dev Allows to registers oracles for a majority vote
    /// @return Is outcome set?
    /// @return Outcome
    function getStatusAndOutcome()
        public
        returns (bool outcomeSet, int256 outcome)
    {
        uint256 i;
        int256[] memory outcomes = new int256[](oracles.length);
        uint256[] memory validations = new uint256[](oracles.length);
        for (i = 0; i < oracles.length; i++)
            if (oracles[i].isOutcomeSet()) {
                int256 _outcome = oracles[i].getOutcome();
                for (uint256 j = 0; j <= i; j++)
                    if (_outcome == outcomes[j]) {
                        validations[j] += 1;
                        break;
                    }
                    else if (validations[j] == 0) {
                        outcomes[j] = _outcome;
                        validations[j] = 1;
                        break;
                    }
            }
        uint256 outcomeValidations = 0;
        uint256 outcomeIndex = 0;
        for (i = 0; i < oracles.length; i++)
            if (validations[i] > outcomeValidations) {
                outcomeValidations = validations[i];
                outcomeIndex = i;
            }
        // There is a majority vote
        if (outcomeValidations * 2 > oracles.length) {
            outcomeSet = true;
            outcome = outcomes[outcomeIndex];
        }
    }

    /// @dev Returns if winning outcome is set
    /// @return Is outcome set?
    function isOutcomeSet()
        public
        constant
        returns (bool)
    {
        var (outcomeSet, ) = getStatusAndOutcome();
        return outcomeSet;
    }

    /// @dev Returns winning outcome
    /// @return Outcome
    function getOutcome()
        public
        constant
        returns (int256)
    {
        var (, winningOutcome) = getStatusAndOutcome();
        return winningOutcome;
    }
}
