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
 * @date 2014
 * Solidity abstract syntax tree.
 */

#include <libsolidity/ast/AST.h>
#include <libsolidity/ast/ASTVisitor.h>
#include <libsolidity/ast/AST_accept.h>

#include <libdevcore/SHA3.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <functional>

using namespace std;
using namespace dev;
using namespace dev::solidity;

class IDDispenser
{
public:
	static size_t next() { return ++instance(); }
	static void reset() { instance() = 0; }
private:
	static size_t& instance()
	{
		static IDDispenser dispenser;
		return dispenser.id;
	}
	size_t id = 0;
};

ASTNode::ASTNode(SourceLocation const& _location):
	m_id(IDDispenser::next()),
	m_location(_location)
{
}

ASTNode::~ASTNode()
{
	delete m_annotation;
}

void ASTNode::resetID()
{
	IDDispenser::reset();
}

ASTAnnotation& ASTNode::annotation() const
{
	if (!m_annotation)
		m_annotation = new ASTAnnotation();
	return *m_annotation;
}

SourceUnitAnnotation& SourceUnit::annotation() const
{
	if (!m_annotation)
		m_annotation = new SourceUnitAnnotation();
	return dynamic_cast<SourceUnitAnnotation&>(*m_annotation);
}

set<SourceUnit const*> SourceUnit::referencedSourceUnits(bool _recurse, set<SourceUnit const*> _skipList) const
{
	set<SourceUnit const*> sourceUnits;
	for (ImportDirective const* importDirective: filteredNodes<ImportDirective>(nodes()))
	{
		auto const& sourceUnit = importDirective->annotation().sourceUnit;
		if (!_skipList.count(sourceUnit))
		{
			_skipList.insert(sourceUnit);
			sourceUnits.insert(sourceUnit);
			if (_recurse)
				sourceUnits += sourceUnit->referencedSourceUnits(true, _skipList);
		}
	}
	return sourceUnits;
}

ImportAnnotation& ImportDirective::annotation() const
{
	if (!m_annotation)
		m_annotation = new ImportAnnotation();
	return dynamic_cast<ImportAnnotation&>(*m_annotation);
}

TypePointer ImportDirective::type() const
{
	solAssert(!!annotation().sourceUnit, "");
	return make_shared<ModuleType>(*annotation().sourceUnit);
}

map<FixedHash<4>, FunctionTypePointer> ContractDefinition::interfaceFunctions() const
{
	auto exportedFunctionList = interfaceFunctionList();

	map<FixedHash<4>, FunctionTypePointer> exportedFunctions;
	for (auto const& it: exportedFunctionList)
		exportedFunctions.insert(it);

	solAssert(
		exportedFunctionList.size() == exportedFunctions.size(),
		"Hash collision at Function Definition Hash calculation"
	);

	return exportedFunctions;
}

FunctionDefinition const* ContractDefinition::constructor() const
{
	for (FunctionDefinition const* f: definedFunctions())
		if (f->isConstructor())
			return f;
	return nullptr;
}

bool ContractDefinition::constructorIsPublic() const
{
	FunctionDefinition const* f = constructor();
	return !f || f->isPublic();
}

FunctionDefinition const* ContractDefinition::fallbackFunction() const
{
	for (ContractDefinition const* contract: annotation().linearizedBaseContracts)
		for (FunctionDefinition const* f: contract->definedFunctions())
			if (f->isFallback())
				return f;
	return nullptr;
}

vector<EventDefinition const*> const& ContractDefinition::interfaceEvents() const
{
	if (!m_interfaceEvents)
	{
		set<string> eventsSeen;
		m_interfaceEvents.reset(new vector<EventDefinition const*>());
		for (ContractDefinition const* contract: annotation().linearizedBaseContracts)
			for (EventDefinition const* e: contract->events())
			{
				/// NOTE: this requires the "internal" version of an Event,
				///       though here internal strictly refers to visibility,
				///       and not to function encoding (jump vs. call)
				auto const& function = e->functionType(true);
				solAssert(function, "");
				string eventSignature = function->externalSignature();
				if (eventsSeen.count(eventSignature) == 0)
				{
					eventsSeen.insert(eventSignature);
					m_interfaceEvents->push_back(e);
				}
			}
	}
	return *m_interfaceEvents;
}

vector<pair<FixedHash<4>, FunctionTypePointer>> const& ContractDefinition::interfaceFunctionList() const
{
	if (!m_interfaceFunctionList)
	{
		set<string> signaturesSeen;
		m_interfaceFunctionList.reset(new vector<pair<FixedHash<4>, FunctionTypePointer>>());
		for (ContractDefinition const* contract: annotation().linearizedBaseContracts)
		{
			vector<FunctionTypePointer> functions;
			for (FunctionDefinition const* f: contract->definedFunctions())
				if (f->isPartOfExternalInterface())
					functions.push_back(make_shared<FunctionType>(*f, false));
			for (VariableDeclaration const* v: contract->stateVariables())
				if (v->isPartOfExternalInterface())
					functions.push_back(make_shared<FunctionType>(*v));
			for (FunctionTypePointer const& fun: functions)
			{
				if (!fun->interfaceFunctionType())
					// Fails hopefully because we already registered the error
					continue;
				string functionSignature = fun->externalSignature();
				if (signaturesSeen.count(functionSignature) == 0)
				{
					signaturesSeen.insert(functionSignature);
					FixedHash<4> hash(dev::keccak256(functionSignature));
					m_interfaceFunctionList->push_back(make_pair(hash, fun));
				}
			}
		}
	}
	return *m_interfaceFunctionList;
}

vector<Declaration const*> const& ContractDefinition::inheritableMembers() const
{
	if (!m_inheritableMembers)
	{
		set<string> memberSeen;
		m_inheritableMembers.reset(new vector<Declaration const*>());
		auto addInheritableMember = [&](Declaration const* _decl)
		{
			solAssert(_decl, "addInheritableMember got a nullpointer.");
			if (memberSeen.count(_decl->name()) == 0 && _decl->isVisibleInDerivedContracts())
			{
				memberSeen.insert(_decl->name());
				m_inheritableMembers->push_back(_decl);
			}
		};

		for (FunctionDefinition const* f: definedFunctions())
			addInheritableMember(f);

		for (VariableDeclaration const* v: stateVariables())
			addInheritableMember(v);

		for (StructDefinition const* s: definedStructs())
			addInheritableMember(s);

		for (EnumDefinition const* e: definedEnums())
			addInheritableMember(e);

		for (EventDefinition const* e: events())
			addInheritableMember(e);
	}
	return *m_inheritableMembers;
}

TypePointer ContractDefinition::type() const
{
	return make_shared<TypeType>(make_shared<ContractType>(*this));
}

ContractDefinitionAnnotation& ContractDefinition::annotation() const
{
	if (!m_annotation)
		m_annotation = new ContractDefinitionAnnotation();
	return dynamic_cast<ContractDefinitionAnnotation&>(*m_annotation);
}

TypeNameAnnotation& TypeName::annotation() const
{
	if (!m_annotation)
		m_annotation = new TypeNameAnnotation();
	return dynamic_cast<TypeNameAnnotation&>(*m_annotation);
}

TypePointer StructDefinition::type() const
{
	return make_shared<TypeType>(make_shared<StructType>(*this));
}

TypeDeclarationAnnotation& StructDefinition::annotation() const
{
	if (!m_annotation)
		m_annotation = new TypeDeclarationAnnotation();
	return dynamic_cast<TypeDeclarationAnnotation&>(*m_annotation);
}

TypePointer EnumValue::type() const
{
	auto parentDef = dynamic_cast<EnumDefinition const*>(scope());
	solAssert(parentDef, "Enclosing Scope of EnumValue was not set");
	return make_shared<EnumType>(*parentDef);
}

TypePointer EnumDefinition::type() const
{
	return make_shared<TypeType>(make_shared<EnumType>(*this));
}

TypeDeclarationAnnotation& EnumDefinition::annotation() const
{
	if (!m_annotation)
		m_annotation = new TypeDeclarationAnnotation();
	return dynamic_cast<TypeDeclarationAnnotation&>(*m_annotation);
}

shared_ptr<FunctionType> FunctionDefinition::functionType(bool _internal) const
{
	if (_internal)
	{
		switch (visibility())
		{
		case Declaration::Visibility::Default:
			solAssert(false, "visibility() should not return Default");
		case Declaration::Visibility::Private:
		case Declaration::Visibility::Internal:
		case Declaration::Visibility::Public:
			return make_shared<FunctionType>(*this, _internal);
		case Declaration::Visibility::External:
			return {};
		default:
			solAssert(false, "visibility() should not return a Visibility");
		}
	}
	else
	{
		switch (visibility())
		{
		case Declaration::Visibility::Default:
			solAssert(false, "visibility() should not return Default");
		case Declaration::Visibility::Private:
		case Declaration::Visibility::Internal:
			return {};
		case Declaration::Visibility::Public:
		case Declaration::Visibility::External:
			return make_shared<FunctionType>(*this, _internal);
		default:
			solAssert(false, "visibility() should not return a Visibility");
		}
	}

	// To make the compiler happy
	return {};
}

TypePointer FunctionDefinition::type() const
{
	return make_shared<FunctionType>(*this);
}

string FunctionDefinition::externalSignature() const
{
	return FunctionType(*this).externalSignature();
}

string FunctionDefinition::fullyQualifiedName() const
{
	auto const* contract = dynamic_cast<ContractDefinition const*>(scope());
	solAssert(contract, "Enclosing scope of function definition was not set.");

	auto fname = name().empty() ? "<fallback>" : name();
	return sourceUnitName() + ":" + contract->name() + "." + fname;
}

FunctionDefinitionAnnotation& FunctionDefinition::annotation() const
{
	if (!m_annotation)
		m_annotation = new FunctionDefinitionAnnotation();
	return dynamic_cast<FunctionDefinitionAnnotation&>(*m_annotation);
}

TypePointer ModifierDefinition::type() const
{
	return make_shared<ModifierType>(*this);
}

ModifierDefinitionAnnotation& ModifierDefinition::annotation() const
{
	if (!m_annotation)
		m_annotation = new ModifierDefinitionAnnotation();
	return dynamic_cast<ModifierDefinitionAnnotation&>(*m_annotation);
}

TypePointer EventDefinition::type() const
{
	return make_shared<FunctionType>(*this);
}

std::shared_ptr<FunctionType> EventDefinition::functionType(bool _internal) const
{
	if (_internal)
		return make_shared<FunctionType>(*this);
	else
		return {};
}

EventDefinitionAnnotation& EventDefinition::annotation() const
{
	if (!m_annotation)
		m_annotation = new EventDefinitionAnnotation();
	return dynamic_cast<EventDefinitionAnnotation&>(*m_annotation);
}

UserDefinedTypeNameAnnotation& UserDefinedTypeName::annotation() const
{
	if (!m_annotation)
		m_annotation = new UserDefinedTypeNameAnnotation();
	return dynamic_cast<UserDefinedTypeNameAnnotation&>(*m_annotation);
}

SourceUnit const& Scopable::sourceUnit() const
{
	ASTNode const* s = scope();
	solAssert(s, "");
	// will not always be a declaratoion
	while (dynamic_cast<Scopable const*>(s) && dynamic_cast<Scopable const*>(s)->scope())
		s = dynamic_cast<Scopable const*>(s)->scope();
	return dynamic_cast<SourceUnit const&>(*s);
}

string Scopable::sourceUnitName() const
{
	return sourceUnit().annotation().path;
}

bool VariableDeclaration::isLValue() const
{
	// External function parameters and constant declared variables are Read-Only
	return !isExternalCallableParameter() && !m_isConstant;
}

bool VariableDeclaration::isLocalVariable() const
{
	auto s = scope();
	return
		dynamic_cast<CallableDeclaration const*>(s) ||
		dynamic_cast<Block const*>(s) ||
		dynamic_cast<ForStatement const*>(s);
}

bool VariableDeclaration::isCallableParameter() const
{
	auto const* callable = dynamic_cast<CallableDeclaration const*>(scope());
	if (!callable)
		return false;
	for (auto const& variable: callable->parameters())
		if (variable.get() == this)
			return true;
	if (callable->returnParameterList())
		for (auto const& variable: callable->returnParameterList()->parameters())
			if (variable.get() == this)
				return true;
	return false;
}

bool VariableDeclaration::isLocalOrReturn() const
{
	return isReturnParameter() || (isLocalVariable() && !isCallableParameter());
}

bool VariableDeclaration::isReturnParameter() const
{
	auto const* callable = dynamic_cast<CallableDeclaration const*>(scope());
	if (!callable)
		return false;
	if (callable->returnParameterList())
		for (auto const& variable: callable->returnParameterList()->parameters())
			if (variable.get() == this)
				return true;
	return false;
}

bool VariableDeclaration::isExternalCallableParameter() const
{
	auto const* callable = dynamic_cast<CallableDeclaration const*>(scope());
	if (!callable || callable->visibility() != Declaration::Visibility::External)
		return false;
	for (auto const& variable: callable->parameters())
		if (variable.get() == this)
			return true;
	return false;
}

bool VariableDeclaration::canHaveAutoType() const
{
	return isLocalVariable() && !isCallableParameter();
}

TypePointer VariableDeclaration::type() const
{
	return annotation().type;
}

shared_ptr<FunctionType> VariableDeclaration::functionType(bool _internal) const
{
	if (_internal)
		return {};
	switch (visibility())
	{
	case Declaration::Visibility::Default:
		solAssert(false, "visibility() should not return Default");
	case Declaration::Visibility::Private:
	case Declaration::Visibility::Internal:
		return {};
	case Declaration::Visibility::Public:
	case Declaration::Visibility::External:
		return make_shared<FunctionType>(*this);
	default:
		solAssert(false, "visibility() should not return a Visibility");
	}

	// To make the compiler happy
	return {};
}

VariableDeclarationAnnotation& VariableDeclaration::annotation() const
{
	if (!m_annotation)
		m_annotation = new VariableDeclarationAnnotation();
	return dynamic_cast<VariableDeclarationAnnotation&>(*m_annotation);
}

StatementAnnotation& Statement::annotation() const
{
	if (!m_annotation)
		m_annotation = new StatementAnnotation();
	return dynamic_cast<StatementAnnotation&>(*m_annotation);
}

InlineAssemblyAnnotation& InlineAssembly::annotation() const
{
	if (!m_annotation)
		m_annotation = new InlineAssemblyAnnotation();
	return dynamic_cast<InlineAssemblyAnnotation&>(*m_annotation);
}

ReturnAnnotation& Return::annotation() const
{
	if (!m_annotation)
		m_annotation = new ReturnAnnotation();
	return dynamic_cast<ReturnAnnotation&>(*m_annotation);
}

VariableDeclarationStatementAnnotation& VariableDeclarationStatement::annotation() const
{
	if (!m_annotation)
		m_annotation = new VariableDeclarationStatementAnnotation();
	return dynamic_cast<VariableDeclarationStatementAnnotation&>(*m_annotation);
}

ExpressionAnnotation& Expression::annotation() const
{
	if (!m_annotation)
		m_annotation = new ExpressionAnnotation();
	return dynamic_cast<ExpressionAnnotation&>(*m_annotation);
}

MemberAccessAnnotation& MemberAccess::annotation() const
{
	if (!m_annotation)
		m_annotation = new MemberAccessAnnotation();
	return dynamic_cast<MemberAccessAnnotation&>(*m_annotation);
}

BinaryOperationAnnotation& BinaryOperation::annotation() const
{
	if (!m_annotation)
		m_annotation = new BinaryOperationAnnotation();
	return dynamic_cast<BinaryOperationAnnotation&>(*m_annotation);
}

FunctionCallAnnotation& FunctionCall::annotation() const
{
	if (!m_annotation)
		m_annotation = new FunctionCallAnnotation();
	return dynamic_cast<FunctionCallAnnotation&>(*m_annotation);
}

IdentifierAnnotation& Identifier::annotation() const
{
	if (!m_annotation)
		m_annotation = new IdentifierAnnotation();
	return dynamic_cast<IdentifierAnnotation&>(*m_annotation);
}

bool Literal::isHexNumber() const
{
	if (token() != Token::Number)
		return false;
	return boost::starts_with(value(), "0x");
}

bool Literal::looksLikeAddress() const
{
	if (subDenomination() != SubDenomination::None)
		return false;

	if (!isHexNumber())
		return false;

	return abs(int(value().length()) - 42) <= 1;
}

bool Literal::passesAddressChecksum() const
{
	solAssert(isHexNumber(), "Expected hex number");
	return dev::passesAddressChecksum(value(), true);
}

std::string Literal::getChecksummedAddress() const
{
	solAssert(isHexNumber(), "Expected hex number");
	/// Pad literal to be a proper hex address.
	string address = value().substr(2);
	if (address.length() > 40)
		return string();
	address.insert(address.begin(), 40 - address.size(), '0');
	return dev::getChecksummedAddress(address);
}
