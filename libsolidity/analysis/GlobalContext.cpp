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
 * @author Gav Wood <g@ethdev.com>
 * @date 2014
 * Container of the (implicit and explicit) global objects.
 */

#include <libsolidity/analysis/GlobalContext.h>

#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/TypeProvider.h>
#include <libsolidity/ast/Types.h>
#include <memory>

using namespace std;

namespace solidity::frontend
{

namespace
{
/// Magic variables get negative ids for easy differentiation
int magicVariableToID(std::string const& _name)
{
	if (_name == "abi") return -1;
	else if (_name == "addmod") return -2;
	else if (_name == "assert") return -3;
	else if (_name == "block") return -4;
	else if (_name == "blockhash") return -5;
	else if (_name == "ecrecover") return -6;
	else if (_name == "gasleft") return -7;
	else if (_name == "keccak256") return -8;
	else if (_name == "msg") return -15;
	else if (_name == "mulmod") return -16;
	else if (_name == "now") return -17;
	else if (_name == "require") return -18;
	else if (_name == "revert") return -19;
	else if (_name == "ripemd160") return -20;
	else if (_name == "selfdestruct") return -21;
	else if (_name == "sha256") return -22;
	else if (_name == "sha3") return -23;
	else if (_name == "suicide") return -24;
	else if (_name == "super") return -25;
	else if (_name == "tx") return -26;
	else if (_name == "type") return -27;
	else if (_name == "this") return -28;
	else if (_name == "ecadd") return -29;
	else if (_name == "ecmul") return -30;
	else if (_name == "ecpairing") return -31;
	else if (_name == "bech32ToAddress") return -32;
	else if (_name == "verifyInclusionAndGetBalance") return -33;
	else if (_name == "verifyPoB") return -34;
	else
		solAssert(false, "Unknown magic variable: \"" + _name + "\".");
}

inline vector<shared_ptr<MagicVariableDeclaration const>> constructMagicVariables()
{
	static auto const magicVarDecl = [](string const& _name, Type const* _type) {
		return make_shared<MagicVariableDeclaration>(magicVariableToID(_name), _name, _type);
	};

    TypePointer g1 = TypeProvider::array(DataLocation::Memory, TypeProvider::uint(256), 2);
    TypePointer g2 = TypeProvider::array(DataLocation::Memory, TypeProvider::uint(256), 4);

	return {
		magicVarDecl("abi", TypeProvider::magic(MagicType::Kind::ABI)),
		magicVarDecl("addmod", TypeProvider::function(strings{"uint", "uint", "uint"}, strings{"uint"}, FunctionType::Kind::AddMod, false, StateMutability::Pure)),
		magicVarDecl("assert", TypeProvider::function(strings{"bool"}, strings{}, FunctionType::Kind::Assert, false, StateMutability::Pure)),
		magicVarDecl("block", TypeProvider::magic(MagicType::Kind::Block)),
		magicVarDecl("blockhash", TypeProvider::function(strings{"uint256"}, strings{"bytes32"}, FunctionType::Kind::BlockHash, false, StateMutability::View)),
		magicVarDecl("ecrecover", TypeProvider::function(strings{"bytes32", "uint8", "bytes32", "bytes32"}, strings{"address"}, FunctionType::Kind::ECRecover, false, StateMutability::Pure)),
        magicVarDecl("ecadd", TypeProvider::function(TypePointers{g1, g1}, TypePointers{g1}, strings{"", ""}, strings{""}, FunctionType::Kind::ECAdd, false, StateMutability::Pure)),
        magicVarDecl("ecmul", TypeProvider::function(TypePointers{g1, TypeProvider::uint(256)}, TypePointers{g1}, strings{"", ""}, strings{""}, FunctionType::Kind::ECMul, false, StateMutability::Pure)),
        magicVarDecl("ecpairing", TypeProvider::function(TypePointers{TypeProvider::array(DataLocation::Memory, g1), TypeProvider::array(DataLocation::Memory, g2)}, TypePointers{TypeProvider::boolean()}, strings{"", ""}, strings{""}, FunctionType::Kind::ECPairing, false, StateMutability::Pure)),
		magicVarDecl("gasleft", TypeProvider::function(strings(), strings{"uint256"}, FunctionType::Kind::GasLeft, false, StateMutability::View)),
		magicVarDecl("keccak256", TypeProvider::function(strings{"bytes memory"}, strings{"bytes32"}, FunctionType::Kind::KECCAK256, false, StateMutability::Pure)),
		magicVarDecl("msg", TypeProvider::magic(MagicType::Kind::Message)),
		magicVarDecl("mulmod", TypeProvider::function(strings{"uint", "uint", "uint"}, strings{"uint"}, FunctionType::Kind::MulMod, false, StateMutability::Pure)),
		magicVarDecl("now", TypeProvider::uint(256)),
		magicVarDecl("require", TypeProvider::function(strings{"bool"}, strings{}, FunctionType::Kind::Require, false, StateMutability::Pure)),
		magicVarDecl("require", TypeProvider::function(strings{"bool", "string memory"}, strings{}, FunctionType::Kind::Require, false, StateMutability::Pure)),
		magicVarDecl("revert", TypeProvider::function(strings(), strings(), FunctionType::Kind::Revert, false, StateMutability::Pure)),
		magicVarDecl("revert", TypeProvider::function(strings{"string memory"}, strings(), FunctionType::Kind::Revert, false, StateMutability::Pure)),
		magicVarDecl("ripemd160", TypeProvider::function(strings{"bytes memory"}, strings{"bytes20"}, FunctionType::Kind::RIPEMD160, false, StateMutability::Pure)),
		magicVarDecl("bech32ToAddress", TypeProvider::function(strings{"string memory"}, strings{"address"}, FunctionType::Kind::BECH32, false, StateMutability::Pure)),
		magicVarDecl("verifyInclusionAndGetBalance", TypeProvider::function(strings{"bytes32", "address", "bytes memory"}, strings{"uint256"}, FunctionType::Kind::VFYINCL, false, StateMutability::Pure)),
		magicVarDecl("verifyPoB", TypeProvider::function(strings{"bytes memory"}, strings{"uint256"}, FunctionType::Kind::VFYPOB, false, StateMutability::Pure)),
		magicVarDecl("selfdestruct", TypeProvider::function(strings{"address payable"}, strings{}, FunctionType::Kind::Selfdestruct)),
		magicVarDecl("sha256", TypeProvider::function(strings{"bytes memory"}, strings{"bytes32"}, FunctionType::Kind::SHA256, false, StateMutability::Pure)),
		magicVarDecl("sha3", TypeProvider::function(strings{"bytes memory"}, strings{"bytes32"}, FunctionType::Kind::KECCAK256, false, StateMutability::Pure)),
		magicVarDecl("suicide", TypeProvider::function(strings{"address payable"}, strings{}, FunctionType::Kind::Selfdestruct)),
		magicVarDecl("tx", TypeProvider::magic(MagicType::Kind::Transaction)),
		// Accepts a MagicType that can be any contract type or an Integer type and returns a
		// MagicType. The TypeChecker handles the correctness of the input and output types.
		magicVarDecl("type", TypeProvider::function(
			strings{},
			strings{},
			FunctionType::Kind::MetaType,
			true,
			StateMutability::Pure
		)),
	};
}

}

GlobalContext::GlobalContext(): m_magicVariables{constructMagicVariables()}
{
}

void GlobalContext::setCurrentContract(ContractDefinition const& _contract)
{
	m_currentContract = &_contract;
}

vector<Declaration const*> GlobalContext::declarations() const
{
	vector<Declaration const*> declarations;
	declarations.reserve(m_magicVariables.size());
	for (ASTPointer<MagicVariableDeclaration const> const& variable: m_magicVariables)
		declarations.push_back(variable.get());
	return declarations;
}

MagicVariableDeclaration const* GlobalContext::currentThis() const
{
	if (!m_thisPointer[m_currentContract])
	{
		Type const* type = TypeProvider::emptyTuple();
		if (m_currentContract)
			type = TypeProvider::contract(*m_currentContract);
		m_thisPointer[m_currentContract] =
			make_shared<MagicVariableDeclaration>(magicVariableToID("this"), "this", type);
	}
	return m_thisPointer[m_currentContract].get();
}

MagicVariableDeclaration const* GlobalContext::currentSuper() const
{
	if (!m_superPointer[m_currentContract])
	{
		Type const* type = TypeProvider::emptyTuple();
		if (m_currentContract)
			type = TypeProvider::typeType(TypeProvider::contract(*m_currentContract, true));
		m_superPointer[m_currentContract] =
			make_shared<MagicVariableDeclaration>(magicVariableToID("super"), "super", type);
	}
	return m_superPointer[m_currentContract].get();
}

}
