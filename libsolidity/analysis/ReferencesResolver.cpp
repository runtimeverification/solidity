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
 * @date 2015
 * Component that resolves type names to types and annotates the AST accordingly.
 */

#include <libsolidity/analysis/ReferencesResolver.h>
#include <libsolidity/ast/AST.h>
#include <libsolidity/analysis/NameAndTypeResolver.h>
#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/analysis/ConstantEvaluator.h>
#include <libsolidity/inlineasm/AsmAnalysis.h>
#include <libsolidity/inlineasm/AsmAnalysisInfo.h>
#include <libsolidity/inlineasm/AsmData.h>
#include <libsolidity/interface/ErrorReporter.h>

#include <boost/algorithm/string.hpp>

using namespace std;
using namespace dev;
using namespace dev::solidity;


bool ReferencesResolver::resolve(ASTNode const& _root)
{
	_root.accept(*this);
	return !m_errorOccurred;
}

bool ReferencesResolver::visit(Block const& _block)
{
	if (!m_resolveInsideCode)
		return false;
	m_experimental050Mode = _block.sourceUnit().annotation().experimentalFeatures.count(ExperimentalFeature::V050);
	// C99-scoped variables
	if (m_experimental050Mode)
		m_resolver.setScope(&_block);
	return true;
}

void ReferencesResolver::endVisit(Block const& _block)
{
	if (!m_resolveInsideCode)
		return;

	// C99-scoped variables
	if (m_experimental050Mode)
		m_resolver.setScope(_block.scope());
}

bool ReferencesResolver::visit(ForStatement const& _for)
{
	if (!m_resolveInsideCode)
		return false;
	m_experimental050Mode = _for.sourceUnit().annotation().experimentalFeatures.count(ExperimentalFeature::V050);
	// C99-scoped variables
	if (m_experimental050Mode)
		m_resolver.setScope(&_for);
	return true;
}

void ReferencesResolver::endVisit(ForStatement const& _for)
{
	if (!m_resolveInsideCode)
		return;
	if (m_experimental050Mode)
		m_resolver.setScope(_for.scope());
}

void ReferencesResolver::endVisit(VariableDeclarationStatement const& _varDeclStatement)
{
	if (!m_resolveInsideCode)
		return;
	if (m_experimental050Mode)
		for (auto const& var: _varDeclStatement.declarations())
			if (var)
				m_resolver.activateVariable(var->name());
}

bool ReferencesResolver::visit(Identifier const& _identifier)
{
	auto declarations = m_resolver.nameFromCurrentScope(_identifier.name());
	if (declarations.empty())
	{
		string suggestions = m_resolver.similarNameSuggestions(_identifier.name());
		string errorMessage =
			"Undeclared identifier." +
			(suggestions.empty()? "": " Did you mean " + std::move(suggestions) + "?");
		declarationError(_identifier.location(), errorMessage);
	}
	else if (declarations.size() == 1)
		_identifier.annotation().referencedDeclaration = declarations.front();
	else
		_identifier.annotation().overloadedDeclarations =
			m_resolver.cleanedDeclarations(_identifier, declarations);
	return false;
}

bool ReferencesResolver::visit(ElementaryTypeName const& _typeName)
{
	_typeName.annotation().type = Type::fromElementaryTypeName(_typeName.typeName());
	return true;
}

bool ReferencesResolver::visit(FunctionDefinition const& _functionDefinition)
{
	m_returnParameters.push_back(_functionDefinition.returnParameterList().get());
	return true;
}

void ReferencesResolver::endVisit(FunctionDefinition const&)
{
	solAssert(!m_returnParameters.empty(), "");
	m_returnParameters.pop_back();
}

bool ReferencesResolver::visit(ModifierDefinition const&)
{
	m_returnParameters.push_back(nullptr);
	return true;
}

void ReferencesResolver::endVisit(ModifierDefinition const&)
{
	solAssert(!m_returnParameters.empty(), "");
	m_returnParameters.pop_back();
}

void ReferencesResolver::endVisit(UserDefinedTypeName const& _typeName)
{
	Declaration const* declaration = m_resolver.pathFromCurrentScope(_typeName.namePath());
	if (!declaration)
	{
		declarationError(_typeName.location(), "Identifier not found or not unique.");
		return;
	}

	_typeName.annotation().referencedDeclaration = declaration;

	if (StructDefinition const* structDef = dynamic_cast<StructDefinition const*>(declaration))
		_typeName.annotation().type = make_shared<StructType>(*structDef);
	else if (EnumDefinition const* enumDef = dynamic_cast<EnumDefinition const*>(declaration))
		_typeName.annotation().type = make_shared<EnumType>(*enumDef);
	else if (ContractDefinition const* contract = dynamic_cast<ContractDefinition const*>(declaration))
		_typeName.annotation().type = make_shared<ContractType>(*contract);
	else
		typeError(_typeName.location(), "Name has to refer to a struct, enum or contract.");
}

void ReferencesResolver::endVisit(FunctionTypeName const& _typeName)
{
	switch (_typeName.visibility())
	{
	case VariableDeclaration::Visibility::Internal:
	case VariableDeclaration::Visibility::External:
		break;
	default:
		typeError(_typeName.location(), "Invalid visibility, can only be \"external\" or \"internal\".");
		return;
	}

	if (_typeName.isPayable() && _typeName.visibility() != VariableDeclaration::Visibility::External)
	{
		typeError(_typeName.location(), "Only external function types can be payable.");
		return;
	}

	if (_typeName.visibility() == VariableDeclaration::Visibility::External)
		for (auto const& t: _typeName.parameterTypes() + _typeName.returnParameterTypes())
		{
			solAssert(t->annotation().type, "Type not set for parameter.");
			if (!t->annotation().type->canBeUsedExternally(false))
			{
				typeError(t->location(), "Internal type cannot be used for external function type.");
				return;
			}
		}

	_typeName.annotation().type = make_shared<FunctionType>(_typeName);
}

void ReferencesResolver::endVisit(Mapping const& _typeName)
{
	TypePointer keyType = _typeName.keyType().annotation().type;
	TypePointer valueType = _typeName.valueType().annotation().type;
	// Convert key type to memory.
	keyType = ReferenceType::copyForLocationIfReference(DataLocation::Memory, keyType);
	// Convert value type to storage reference.
	valueType = ReferenceType::copyForLocationIfReference(DataLocation::Storage, valueType);
	_typeName.annotation().type = make_shared<MappingType>(keyType, valueType);
}

void ReferencesResolver::endVisit(ArrayTypeName const& _typeName)
{
	TypePointer baseType = _typeName.baseType().annotation().type;
	if (!baseType)
	{
		solAssert(!m_errorReporter.errors().empty(), "");
		return;
	}
	if (baseType->storageBytes() == 0)
		fatalTypeError(_typeName.baseType().location(), "Illegal base type of storage size zero for array.");
	if (Expression const* length = _typeName.length())
	{
		TypePointer lengthTypeGeneric = length->annotation().type;
		if (!lengthTypeGeneric)
			lengthTypeGeneric = ConstantEvaluator(m_errorReporter).evaluate(*length);
		RationalNumberType const* lengthType = dynamic_cast<RationalNumberType const*>(lengthTypeGeneric.get());
		if (!lengthType || !lengthType->mobileType())
			fatalTypeError(length->location(), "Invalid array length, expected integer literal or constant expression.");
		else if (lengthType->isFractional())
			fatalTypeError(length->location(), "Array with fractional length specified.");
		else if (lengthType->isNegative())
			fatalTypeError(length->location(), "Array with negative length specified.");
		else
			_typeName.annotation().type = make_shared<ArrayType>(DataLocation::Storage, baseType, u256(lengthType->literalValue(nullptr)));
	}
	else
		_typeName.annotation().type = make_shared<ArrayType>(DataLocation::Storage, baseType);
}

bool ReferencesResolver::visit(InlineAssembly const& _inlineAssembly)
{
	m_resolver.warnVariablesNamedLikeInstructions();

	// Errors created in this stage are completely ignored because we do not yet know
	// the type and size of external identifiers, which would result in false errors.
	// The only purpose of this step is to fill the inline assembly annotation with
	// external references.
	ErrorList errors;
	ErrorReporter errorsIgnored(errors);
	julia::ExternalIdentifierAccess::Resolver resolver =
	[&](assembly::Identifier const& _identifier, julia::IdentifierContext, bool _crossesFunctionBoundary) {
		auto declarations = m_resolver.nameFromCurrentScope(_identifier.name);
		bool isSlot = boost::algorithm::ends_with(_identifier.name, "_slot");
		bool isOffset = boost::algorithm::ends_with(_identifier.name, "_offset");
		if (isSlot || isOffset)
		{
			// special mode to access storage variables
			if (!declarations.empty())
				// the special identifier exists itself, we should not allow that.
				return size_t(-1);
			string realName = _identifier.name.substr(0, _identifier.name.size() - (
				isSlot ?
				string("_slot").size() :
				string("_offset").size()
			));
			declarations = m_resolver.nameFromCurrentScope(realName);
		}
		if (declarations.size() != 1)
			return size_t(-1);
		if (auto var = dynamic_cast<VariableDeclaration const*>(declarations.front()))
			if (var->isLocalVariable() && _crossesFunctionBoundary)
			{
				declarationError(_identifier.location, "Cannot access local Solidity variables from inside an inline assembly function.");
				return size_t(-1);
			}
		_inlineAssembly.annotation().externalReferences[&_identifier].isSlot = isSlot;
		_inlineAssembly.annotation().externalReferences[&_identifier].isOffset = isOffset;
		_inlineAssembly.annotation().externalReferences[&_identifier].declaration = declarations.front();
		return size_t(1);
	};

	// Will be re-generated later with correct information
	// We use the latest EVM version because we will re-run it anyway.
	assembly::AsmAnalysisInfo analysisInfo;
	boost::optional<Error::Type> errorTypeForLoose = m_experimental050Mode ? Error::Type::SyntaxError : Error::Type::Warning;
	assembly::AsmAnalyzer(analysisInfo, errorsIgnored, EVMVersion(), errorTypeForLoose, assembly::AsmFlavour::Loose, resolver).analyze(_inlineAssembly.operations());
	return false;
}

bool ReferencesResolver::visit(Return const& _return)
{
	solAssert(!m_returnParameters.empty(), "");
	_return.annotation().functionReturnParameters = m_returnParameters.back();
	return true;
}

void ReferencesResolver::endVisit(VariableDeclaration const& _variable)
{
	if (_variable.annotation().type)
		return;

	TypePointer type;
	if (_variable.typeName())
	{
		type = _variable.typeName()->annotation().type;
		using Location = VariableDeclaration::Location;
		Location varLoc = _variable.referenceLocation();
		DataLocation typeLoc = DataLocation::Memory;
		// References are forced to calldata for external function parameters (not return)
		// and memory for parameters (also return) of publicly visible functions.
		// They default to memory for function parameters and storage for local variables.
		// As an exception, "storage" is allowed for library functions.
		if (auto ref = dynamic_cast<ReferenceType const*>(type.get()))
		{
			bool isPointer = true;
			if (_variable.isExternalCallableParameter())
			{
				auto const& contract = dynamic_cast<ContractDefinition const&>(
					*dynamic_cast<Declaration const&>(*_variable.scope()).scope()
				);
				if (contract.isLibrary())
				{
					if (varLoc == Location::Memory)
						fatalTypeError(_variable.location(),
							"Location has to be calldata or storage for external "
							"library functions (remove the \"memory\" keyword)."
						);
				}
				else
				{
					// force location of external function parameters (not return) to calldata
					if (varLoc != Location::Default)
						fatalTypeError(_variable.location(),
							"Location has to be calldata for external functions "
							"(remove the \"memory\" or \"storage\" keyword)."
						);
				}
				if (varLoc == Location::Default)
					typeLoc = DataLocation::Memory;
				else
					typeLoc = varLoc == Location::Memory ? DataLocation::Memory : DataLocation::Storage;
			}
			else if (_variable.isCallableParameter() && dynamic_cast<Declaration const&>(*_variable.scope()).isPublic())
			{
				auto const& contract = dynamic_cast<ContractDefinition const&>(
					*dynamic_cast<Declaration const&>(*_variable.scope()).scope()
				);
				// force locations of public or external function (return) parameters to memory
				if (varLoc == Location::Storage && !contract.isLibrary())
					fatalTypeError(_variable.location(),
						"Location has to be memory for publicly visible functions "
						"(remove the \"storage\" keyword)."
					);
				if (varLoc == Location::Default || !contract.isLibrary())
					typeLoc = DataLocation::Memory;
				else
					typeLoc = varLoc == Location::Memory ? DataLocation::Memory : DataLocation::Storage;
			}
			else
			{
				if (_variable.isConstant())
				{
					if (varLoc != Location::Default && varLoc != Location::Memory)
						fatalTypeError(
							_variable.location(),
							"Storage location has to be \"memory\" (or unspecified) for constants."
						);
					typeLoc = DataLocation::Memory;
				}
				else if (varLoc == Location::Default)
				{
					if (_variable.isCallableParameter())
						typeLoc = DataLocation::Memory;
					else
					{
						typeLoc = DataLocation::Storage;
						if (_variable.isLocalVariable())
						{
							if (_variable.sourceUnit().annotation().experimentalFeatures.count(ExperimentalFeature::V050))
								typeError(
									_variable.location(),
									"Storage location must be specified as either \"memory\" or \"storage\"."
								);
							else
								m_errorReporter.warning(
									_variable.location(),
									"Variable is declared as a storage pointer. "
									"Use an explicit \"storage\" keyword to silence this warning."
								);
						}
					}
				}
				else
					typeLoc = varLoc == Location::Memory ? DataLocation::Memory : DataLocation::Storage;
				isPointer = !_variable.isStateVariable();
			}

			type = ref->copyForLocation(typeLoc, isPointer);
		}
		else if (varLoc != Location::Default && !ref)
			typeError(_variable.location(), "Storage location can only be given for array or struct types.");

		_variable.annotation().type = type;
	}
	else if (!_variable.canHaveAutoType())
		typeError(_variable.location(), "Explicit type needed.");
	// otherwise we have a "var"-declaration whose type is resolved by the first assignment
}

void ReferencesResolver::typeError(SourceLocation const& _location, string const& _description)
{
	m_errorOccurred = true;
	m_errorReporter.typeError(_location, _description);
}

void ReferencesResolver::fatalTypeError(SourceLocation const& _location, string const& _description)
{
	m_errorOccurred = true;
	m_errorReporter.fatalTypeError(_location, _description);
}

void ReferencesResolver::declarationError(SourceLocation const& _location, string const& _description)
{
	m_errorOccurred = true;
	m_errorReporter.declarationError(_location, _description);
}

void ReferencesResolver::fatalDeclarationError(SourceLocation const& _location, string const& _description)
{
	m_errorOccurred = true;
	m_errorReporter.fatalDeclarationError(_location, _description);
}
