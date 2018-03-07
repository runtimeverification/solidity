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
/**
 * @author Christian <c@ethdev.com>
 * @date 2016
 * Framework for executing contracts and testing them using RPC.
 */

#include <cstdlib>
#include <boost/test/framework.hpp>
#include <libdevcore/CommonIO.h>
#include <test/ExecutionFramework.h>

#include <boost/algorithm/string/replace.hpp>

using namespace std;
using namespace dev;
using namespace dev::test;

namespace // anonymous
{

h256 const EmptyTrie("0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421");

string getIPCSocketPath()
{
	string ipcPath = dev::test::Options::get().ipcPath;
	if (ipcPath.empty())
		BOOST_FAIL("ERROR: ipcPath not set! (use --ipcpath <path> or the environment variable ETH_TEST_IPC)");

	return ipcPath;
}

}

ExecutionFramework::ExecutionFramework() :
	m_rpc(RPCSession::instance(getIPCSocketPath())),
	m_optimize(dev::test::Options::get().optimize),
	m_showMessages(dev::test::Options::get().showMessages),
	m_sender(m_rpc.account(0))
{
	m_rpc.test_rewindToBlock(0);
}

std::pair<bool, string> ExecutionFramework::compareAndCreateMessage(
	std::vector<bytes> const& _result,
	std::vector<bytes> const& _expectation
)
{
	if (_result == _expectation)
		return std::make_pair(true, std::string{});
	std::string message =
			"Invalid encoded data\n";
	return make_pair(false, message);
}

void ExecutionFramework::sendMessage(std::vector<bytes> const& _arguments, std::string _function, bytes const& _data, bool _isCreation, u256 const& _value)
{
	// TODO: implement for IELE
}

void ExecutionFramework::sendEther(Address const& _to, u256 const& _value)
{
	RPCSession::TransactionData d;
	d.data = "0x";
	d.from = "0x" + toString(m_sender);
	d.gas = toHex(m_gas, HexPrefix::Add);
	d.gasPrice = toHex(m_gasPrice, HexPrefix::Add);
	d.value = toHex(_value, HexPrefix::Add);
	d.to = dev::toString(_to);

	string txHash = m_rpc.eth_sendTransaction(d);
	m_rpc.test_mineBlocks(1);
}

size_t ExecutionFramework::currentTimestamp()
{
	auto latestBlock = m_rpc.eth_getBlockByNumber("latest", false);
	return size_t(u256(latestBlock.get("timestamp", "invalid").asString()));
}

size_t ExecutionFramework::blockTimestamp(u256 _number)
{
	auto latestBlock = m_rpc.eth_getBlockByNumber(toString(_number), false);
	return size_t(u256(latestBlock.get("timestamp", "invalid").asString()));
}

Address ExecutionFramework::account(size_t _i)
{
	return Address(m_rpc.accountCreateIfNotExists(_i));
}

bool ExecutionFramework::addressHasCode(Address const& _addr)
{
	string code = m_rpc.eth_getCode(toString(_addr), "latest");
	return !code.empty() && code != "0x";
}

u256 ExecutionFramework::balanceAt(Address const& _addr)
{
	return u256(m_rpc.eth_getBalance(toString(_addr), "latest"));
}

bool ExecutionFramework::storageEmpty(Address const& _addr)
{
	h256 root(m_rpc.eth_getStorageRoot(toString(_addr), "latest"));
	BOOST_CHECK(root);
	return root == EmptyTrie;
}
