pragma solidity ^0.4.11;

contract SimpleAuction {
    // Parameters of the auction. Times are either
    // absolute unix timestamps (seconds since 1970-01-01)
    // or time periods in seconds.
    address public beneficiary;
    uint public auctionEnd;

    // Current state of the auction.
    address public highestBidder;
    uint public highestBid;

    // Set to true at the end, disallows any change
    bool ended;

    // Create a simple auction with `_biddingTime`
    // seconds bidding time on behalf of the
    // beneficiary address `_beneficiary`.
    constructor(
        address _beneficiary,
        uint _biddingTime
    ) public {
        beneficiary = _beneficiary;
        auctionEnd = now + _biddingTime;
    }

    // Bid on the auction with the value sent
    // together with this transaction.
    // The value will only be refunded if the
    // auction is not won.
    function bid() public payable {
        // No arguments are necessary, all
        // information is already part of
        // the transaction. The keyword payable
        // is required for the function to
        // be able to receive Ether.

        // Revert the call if the bidding
        // period is over.
        require(now <= auctionEnd);

        // If the bid is not higher, send the
        // money back.
        require(msg.value > highestBid);

        if (highestBidder != 0) {
            highestBidder.transfer(highestBid);
        }
        highestBidder = msg.sender;
        highestBid = msg.value;
    }

    // End the auction and send the highest bid
    // to the beneficiary.
    function settleAuction() public returns (address) {
        require(now >= auctionEnd); // auction did not yet end
        require(!ended); // this function has already been called

        ended = true;

        beneficiary.transfer(highestBid);

        return highestBidder;
    }
}
