/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * @author Christian <c@ethdev.com>
 * @date 2016
 * Framework for executing contracts and testing them using RPC.
 */

#include <test/ExecutionFramework.h>

#include <test/EVMHost.h>

#include <test/evmc/evmc.hpp>

#include <libsolutil/CommonIO.h>
#include <libsolutil/FunctionSelector.h>

#include <liblangutil/Exceptions.h>

#include <boost/test/framework.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <cstdlib>

using namespace std;
using namespace solidity;
using namespace solidity::util;
using namespace solidity::test;


namespace // anonymous
{

string getIPCSocketPath()
{
    const boost::filesystem::path &ipcPath = solidity::test::CommonOptions::get().ipcPath;
    if (ipcPath.empty())
        BOOST_FAIL("ERROR: ipcPath not set! (use --ipcpath <path> or the environment variable ETH_TEST_IPC)");

    return ipcPath.string();
}

}

ExecutionFramework::ExecutionFramework():
	ExecutionFramework(solidity::test::CommonOptions::get().evmVersion(), solidity::test::CommonOptions::get().vmPaths)
{
}

ExecutionFramework::ExecutionFramework(langutil::EVMVersion _evmVersion, vector<boost::filesystem::path> const& _vmPaths):
	m_rpc(RPCSession::instance(getIPCSocketPath(), solidity::test::CommonOptions::get().walletId(), solidity::test::CommonOptions::get().privateFromAddr())),
	m_evmVersion(_evmVersion),
	m_optimiserSettings(solidity::frontend::OptimiserSettings::minimal()),
	m_showMessages(solidity::test::CommonOptions::get().showMessages),
	m_vmPaths(_vmPaths)
{
	if (solidity::test::CommonOptions::get().optimize)
		m_optimiserSettings = solidity::frontend::OptimiserSettings::standard();

    m_rpc.test_rewindToBlock(1);

/*
	for (auto const& path: m_vmPaths)
		if (EVMHost::getVM(path.string()).has_capability(EVMC_CAPABILITY_EWASM))
			m_supportsEwasm = true;

	selectVM(evmc_capabilities::EVMC_CAPABILITY_EVM1);
*/
}

void ExecutionFramework::selectVM(evmc_capabilities _cap)
{
	m_evmcHost.reset();
	for (auto const& path: m_vmPaths)
	{
		evmc::VM& vm = EVMHost::getVM(path.string());
		if (vm.has_capability(_cap))
		{
			m_evmcHost = make_unique<EVMHost>(m_evmVersion, vm);
			break;
		}
	}
	solAssert(m_evmcHost != nullptr, "");
	reset();
}

void ExecutionFramework::reset()
{
	m_evmcHost->reset();
	for (size_t i = 0; i < 10; i++)
		m_evmcHost->accounts[EVMHost::convertToEVMC(account(i))].balance =
			EVMHost::convertToEVMC(u256(1) << 100);
}

std::pair<bool, string> ExecutionFramework::compareAndCreateMessage(
	bytes const& _result,
	bytes const& _expectation
)
{
	return compareAndCreateMessage(std::vector<bytes>(1, _result), std::vector<bytes>(1, _expectation));
}

std::pair<bool, string> ExecutionFramework::compareAndCreateMessage(
	std::vector<bytes> const& _result,
	std::vector<bytes> const& _expectation
)
{
	if (_result == _expectation)
		return std::make_pair(true, std::string{});
	std::string message =
			"Invalid encoded data:\nExpected: [ ";
	for (bytes const& val : _expectation) {
		message += toHex(val) + " ";
	}
	message += "]\nActual:   [ ";
	for (bytes const& val : _result) {
		message += toHex(val) + " ";
	}
	message += "]\n";
	return make_pair(false, message);
}

bytes ExecutionFramework::panicData(util::PanicCode _code)
{
	return
/*
		m_evmVersion.supportsReturndata() ?
		toCompactBigEndian(selectorFromSignature32("Panic(uint256)"), 4) + encode(u256(_code)) :
*/
		bytes();
}

u256 ExecutionFramework::gasLimit() const
{
	return {m_evmcHost->tx_context.block_gas_limit};
}

u256 ExecutionFramework::gasPrice() const
{
	// here and below we use "return u256{....}" instead of just "return {....}"
	// to please MSVC and avoid unexpected
	// warning C4927 : illegal conversion; more than one user - defined conversion has been implicitly applied
	return u256{EVMHost::convertFromEVMC(m_evmcHost->tx_context.tx_gas_price)};
}

u256 ExecutionFramework::blockHash(u256 const& _number) const
{
	return u256{EVMHost::convertFromEVMC(
		m_evmcHost->get_block_hash(static_cast<int64_t>(_number & numeric_limits<uint64_t>::max()))
	)};
}

u256 ExecutionFramework::blockNumber() const
{
	return m_blockNumber;
}

void ExecutionFramework::sendMessage(std::vector<bytes> const& _arguments, std::string _function, bytes const& _data, bool _isCreation, u256 const& _value)
{
	if (m_showMessages)
	{
	        if (_isCreation) {
	                cout << "CREATE " << m_sender.hex() << ":" << endl;
			cout << " code:      " << toHex(_data) << endl;
		}
	        else {
	                cout << "CALL   " << m_sender.hex() << " -> " << m_contractAddress.hex() << ":" << endl;
			cout << " function:  " << _function << endl;
		}
	        if (_value > 0)
	                cout << " value: " << _value << endl;
		cout << " args:      [ ";
		for (bytes arg : _arguments) {
			cout << toHex(arg) << " ";
		}
		cout << "]" << endl;
	}
	RPCSession::TransactionData d;
	d.data = "0x" + toHex(_data);
	for (bytes const& arg : _arguments) {
		d.arguments.push_back("0x" + toHex(arg));
	}
	d.function = _function;
	d.from = "0x" + toString(m_sender);
	d.gas = toHex(m_gas, HexPrefix::Add);
	d.gasPrice = toHex(m_gasPrice, HexPrefix::Add);
	d.value = toHex(_value, HexPrefix::Add);
	if (!_isCreation)
	{
	        d.to = "0x" + toString(m_contractAddress);
	        BOOST_REQUIRE(m_rpc.eth_getCode(d.to, "latest").size() > 2);
	}

	string txHash = m_rpc.iele_sendTransaction(d);
	m_rpc.test_mineBlocks(1);
	RPCSession::TransactionReceipt receipt(m_rpc.eth_getTransactionReceipt(txHash));

	m_blockNumber = u256(receipt.blockNumber);
	m_status = bigint(receipt.status);

	m_gasUsed = u256(receipt.gasUsed);
	m_transactionSuccessful = m_status == 0;

	if (_isCreation)
	{
	        m_contractAddress = h160(receipt.contractAddress);
	}
        else
        {
		vector<string> const& outputs = receipt.returnData;
		m_output.clear();
		for (auto const& output : outputs) {
			m_output.push_back(asBytes(output));
		}
        }


	if (m_showMessages)
	{
	        cout << " out:     [ ";
		for (bytes const& output : m_output) {
			cout << toHex(output) << " ";
		}
		cout << "]" << endl;
	        cout << " tx hash: " << txHash << endl;
		cout << " gas used: " << m_gasUsed.str() << endl;
	}

    m_logs.clear();
    for (auto const& log: receipt.logEntries)
    {
        LogEntry entry;
        entry.address = h160(log.address);
        for (auto const& topic: log.topics)
            entry.topics.push_back(h256(topic));
        entry.data = fromHex(log.data, WhenError::Throw);
        m_logs.push_back(entry);
    }
}

void ExecutionFramework::sendEther(h160 const& _to, u256 const& _value)
{
	RPCSession::TransactionData d;
	d.data = "0x";
	d.function = "deposit";
	d.from = "0x" + toString(m_sender);
	d.gas = toHex(m_gas, HexPrefix::Add);
	d.gasPrice = toHex(m_gasPrice, HexPrefix::Add);
	d.value = toHex(_value, HexPrefix::Add);
	d.to = "0x" + toString(_to);

	string txHash = m_rpc.iele_sendTransaction(d);
	m_rpc.test_mineBlocks(1);
}

size_t ExecutionFramework::currentTimestamp()
{
	auto timestamp = m_rpc.eth_getTimestamp("latest");
	return size_t(u256(timestamp));
}

size_t ExecutionFramework::blockTimestamp(u256 _block)
{
	auto timestamp = m_rpc.eth_getTimestamp(toString(_block));
	return size_t(u256(timestamp));
}

h160 ExecutionFramework::account(size_t _i)
{
	return h160(m_rpc.accountCreateIfNotExists(_i));;
}

bool ExecutionFramework::addressHasCode(h160 const& _addr)
{
    string code = m_rpc.eth_getCode("0x" + toString(_addr), "latest");
    return !code.empty() && code != "0x";
}

size_t ExecutionFramework::numLogs() const
{
	return m_logs.size();
}

size_t ExecutionFramework::numLogTopics(size_t _logIdx) const
{
	return m_logs.at(_logIdx).topics.size();
}

h256 ExecutionFramework::logTopic(size_t _logIdx, size_t _topicIdx) const
{
	return m_logs.at(_logIdx).topics.at(_topicIdx);
}

h160 ExecutionFramework::logAddress(size_t _logIdx) const
{
	return m_logs.at(_logIdx).address;
}

bytes ExecutionFramework::logData(size_t _logIdx) const
{
	const auto& data = m_logs.at(_logIdx).data;
	// TODO: Return a copy of log data, because this is expected from REQUIRE_LOG_DATA(),
	//       but reference type like string_view would be preferable.
	return {data.begin(), data.end()};
}

u256 ExecutionFramework::balanceAt(h160 const& _addr)
{
	return u256(m_rpc.eth_getBalance("0x" + toString(_addr), "latest"));
}

bool ExecutionFramework::storageEmpty(h160 const& _addr)
{
	return m_rpc.eth_isStorageEmpty("0x" + toString(_addr), "latest");
}
