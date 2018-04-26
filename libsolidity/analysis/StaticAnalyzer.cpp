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
 * @author Federico Bond <federicobond@gmail.com>
 * @date 2016
 * Static analyzer and checker.
 */

#include <libsolidity/analysis/StaticAnalyzer.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/interface/ErrorReporter.h>
#include <memory>

using namespace std;
using namespace dev;
using namespace dev::solidity;

bool StaticAnalyzer::analyze(SourceUnit const& _sourceUnit)
{
	_sourceUnit.accept(*this);
	return Error::containsOnlyWarnings(m_errorReporter.errors());
}

bool StaticAnalyzer::visit(ContractDefinition const& _contract)
{
	m_library = _contract.isLibrary();
	m_currentContract = &_contract;
	return true;
}

void StaticAnalyzer::endVisit(ContractDefinition const&)
{
	m_library = false;
	m_currentContract = nullptr;
}

bool StaticAnalyzer::visit(FunctionDefinition const& _function)
{
	if (_function.isImplemented())
		m_currentFunction = &_function;
	else
		solAssert(!m_currentFunction, "");
	solAssert(m_localVarUseCount.empty(), "");
	m_nonPayablePublic = _function.isPublic() && !_function.isPayable();
	m_constructor = _function.isConstructor();
	return true;
}

void StaticAnalyzer::endVisit(FunctionDefinition const&)
{
	m_currentFunction = nullptr;
	m_nonPayablePublic = false;
	m_constructor = false;
	for (auto const& var: m_localVarUseCount)
		if (var.second == 0)
		{
			if (var.first->isCallableParameter())
				m_errorReporter.warning(
					var.first->location(),
					"Unused function parameter. Remove or comment out the variable name to silence this warning."
				);
			else
				m_errorReporter.warning(var.first->location(), "Unused local variable.");
		}

	m_localVarUseCount.clear();
}

bool StaticAnalyzer::visit(Identifier const& _identifier)
{
	if (m_currentFunction)
		if (auto var = dynamic_cast<VariableDeclaration const*>(_identifier.annotation().referencedDeclaration))
		{
			solAssert(!var->name().empty(), "");
			if (var->isLocalVariable())
				m_localVarUseCount[var] += 1;
		}
	return true;
}

bool StaticAnalyzer::visit(VariableDeclaration const& _variable)
{
	if (m_currentFunction)
	{
		solAssert(_variable.isLocalVariable(), "");
		if (_variable.name() != "")
			// This is not a no-op, the entry might pre-exist.
			m_localVarUseCount[&_variable] += 0;
	}
	else if (_variable.isStateVariable())
	{
		set<StructDefinition const*> structsSeen;
		if (structureSizeEstimate(*_variable.type(), structsSeen) >= bigint(1) << 64)
			m_errorReporter.warning(
				_variable.location(),
				"Variable covers a large part of storage and thus makes collisions likely. "
				"Either use mappings or dynamic arrays and allow their size to be increased only "
				"in small quantities per transaction."
			);
	}
	return true;
}

bool StaticAnalyzer::visit(Return const& _return)
{
	// If the return has an expression, it counts as
	// a "use" of the return parameters.
	if (m_currentFunction && _return.expression())
		for (auto const& var: m_currentFunction->returnParameters())
			if (!var->name().empty())
				m_localVarUseCount[var.get()] += 1;
	return true;
}

bool StaticAnalyzer::visit(ExpressionStatement const& _statement)
{
	if (_statement.expression().annotation().isPure)
		m_errorReporter.warning(
			_statement.location(),
			"Statement has no effect."
		);

	return true;
}

bool StaticAnalyzer::visit(MemberAccess const& _memberAccess)
{
	bool const v050 = m_currentContract->sourceUnit().annotation().experimentalFeatures.count(ExperimentalFeature::V050);

	if (MagicType const* type = dynamic_cast<MagicType const*>(_memberAccess.expression().annotation().type.get()))
		if (type->kind() == MagicType::Kind::Message && _memberAccess.memberName() == "gas")
		{
			if (v050)
				m_errorReporter.typeError(
					_memberAccess.location(),
					"\"msg.gas\" has been deprecated in favor of \"gasleft()\""
				);
			else
				m_errorReporter.warning(
					_memberAccess.location(),
					"\"msg.gas\" has been deprecated in favor of \"gasleft()\""
				);
		}

	if (m_nonPayablePublic && !m_library)
		if (MagicType const* type = dynamic_cast<MagicType const*>(_memberAccess.expression().annotation().type.get()))
			if (type->kind() == MagicType::Kind::Message && _memberAccess.memberName() == "value")
				m_errorReporter.warning(
					_memberAccess.location(),
					"\"msg.value\" used in non-payable function. Do you want to add the \"payable\" modifier to this function?"
				);

	if (_memberAccess.memberName() == "callcode")
		if (auto const* type = dynamic_cast<FunctionType const*>(_memberAccess.annotation().type.get()))
			if (type->kind() == FunctionType::Kind::BareCallCode)
			{
				if (v050)
					m_errorReporter.typeError(
						_memberAccess.location(),
						"\"callcode\" has been deprecated in favour of \"delegatecall\"."
					);
				else
					m_errorReporter.warning(
						_memberAccess.location(),
						"\"callcode\" has been deprecated in favour of \"delegatecall\"."
					);
			}

	if (m_constructor && m_currentContract)
		if (ContractType const* type = dynamic_cast<ContractType const*>(_memberAccess.expression().annotation().type.get()))
			if (type->contractDefinition() == *m_currentContract)
				m_errorReporter.warning(_memberAccess.location(), "\"this\" used in constructor.");

	return true;
}

bool StaticAnalyzer::visit(InlineAssembly const& _inlineAssembly)
{
	if (!m_currentFunction)
		return true;

	for (auto const& ref: _inlineAssembly.annotation().externalReferences)
	{
		if (auto var = dynamic_cast<VariableDeclaration const*>(ref.second.declaration))
		{
			solAssert(!var->name().empty(), "");
			if (var->isLocalVariable())
				m_localVarUseCount[var] += 1;
		}
	}

	return true;
}

bigint StaticAnalyzer::structureSizeEstimate(Type const& _type, set<StructDefinition const*>& _structsSeen)
{
	switch (_type.category())
	{
	case Type::Category::Array:
	{
		auto const& t = dynamic_cast<ArrayType const&>(_type);
		return structureSizeEstimate(*t.baseType(), _structsSeen) * (t.isDynamicallySized() ? 1 : t.length());
	}
	case Type::Category::Struct:
	{
		auto const& t = dynamic_cast<StructType const&>(_type);
		bigint size = 1;
		if (!_structsSeen.count(&t.structDefinition()))
		{
			_structsSeen.insert(&t.structDefinition());
			for (auto const& m: t.members(nullptr))
				size += structureSizeEstimate(*m.type, _structsSeen);
		}
		return size;
	}
	case Type::Category::Mapping:
	{
		return structureSizeEstimate(*dynamic_cast<MappingType const&>(_type).valueType(), _structsSeen);
	}
	default:
		break;
	}
	return bigint(1);
}
