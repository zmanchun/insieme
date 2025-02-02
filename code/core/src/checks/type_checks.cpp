/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details
 * regarding third party software licenses.
 */

#include "insieme/core/checks/type_checks.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/ir++_utils.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"
#include "insieme/core/types/subtyping.h"
#include "insieme/core/types/type_variable_deduction.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/lang/enum.h"

#include "insieme/core/lang/array.h"
#include "insieme/core/lang/channel.h"
#include "insieme/core/lang/reference.h"
#include "insieme/core/lang/pointer.h"

#include "insieme/utils/numeric_cast.h"

namespace insieme {
namespace core {
namespace checks {


	OptionalMessageList KeywordCheck::visitGenericType(const GenericTypeAddress& address) {
		OptionalMessageList res;

		if((address->getName()->getValue() == "array" && !lang::isArray(address)) || (address->getName()->getValue() == "ref" && !lang::isReference(address))
		   || (address->getName()->getValue() == "channel" && !lang::isChannel(address))) {
			add(res, Message(address, EC_TYPE_ILLEGAL_USE_OF_TYPE_KEYWORD, format("Name of generic type %s is a reserved keyword.", *address),
			                 Message::WARNING));
		}
		return res;
	}

	OptionalMessageList FunctionKindCheck::visitFunctionType(const FunctionTypeAddress& address) {
		OptionalMessageList res;

		// check value of kind-flag (must be within the enumeration)
		switch(address->getKind()) {
		case FK_PLAIN:
		case FK_CLOSURE:
		case FK_CONSTRUCTOR:
		case FK_DESTRUCTOR:
		case FK_MEMBER_FUNCTION:
		case FK_VIRTUAL_MEMBER_FUNCTION: break; // all valid values
		default:
			// this is an invalid value
			add(res, Message(address, EC_TYPE_ILLEGAL_FUNCTION_TYPE_KIND,
			                 format("Invalid value for function-type kind field: %d", toString(address->getKind())), Message::ERROR));
		}

		// check object type for ctors / dtors / member functions
		if(address->isConstructor() || address->isDestructor() || address->isMemberFunction() || address->isVirtualMemberFunction()) {
			if(address->getParameterTypes().empty()) {
				add(res, Message(address, EC_TYPE_ILLEGAL_OBJECT_TYPE, format("Missing object type within ctor / dtor / member function."), Message::ERROR));
			} else if(!analysis::isObjectReferenceType(address->getParameterType(0))) {
				add(res, Message(address, EC_TYPE_ILLEGAL_OBJECT_TYPE, format("Invalid type for target object: %s", toString(address->getParameterType(0))),
				                 Message::ERROR));
			}
		}

		// check no-arguments for destructor
		if(address->isDestructor()) {
			if(address->getParameterTypes().size() > 1u) {
				add(res, Message(address, EC_TYPE_ILLEGAL_DESTRUCTOR_PARAMETERS, format("Destructor type must not exhibit parameters!"), Message::ERROR));
			}
		}

		// check return type of constructor
		if(address->isConstructor() && !address->getParameterTypes().empty()) {
			if(*address->getParameterType(0) != *address->getReturnType()) {
				add(res, Message(address, EC_TYPE_ILLEGAL_CONSTRUCTOR_RETURN_TYPE,
				                 format("Invalid return type of constructor - is: %s, should %s", toString(*address->getReturnType()),
				                        toString(*address->getParameterType(0))),
				                 Message::ERROR));
			}
		}

		// check return type of destructor
		if(address->isDestructor() && !address->getParameterTypes().empty()) {
			if(*address->getParameterType(0) != *address->getReturnType()) {
				add(res, Message(address, EC_TYPE_ILLEGAL_DESTRUCTOR_RETURN_TYPE,
				                 format("Invalid return type of destructor - is: %s, should %s", toString(*address->getReturnType()),
				                        toString(*address->getParameterType(0))),
				                 Message::ERROR));
			}
		}

		return res;
	}


	OptionalMessageList ParentCheck::visitParent(const ParentAddress& address) {
		OptionalMessageList res;

		// just check whether parent type is a potential object type
		auto type = address.as<ParentPtr>()->getType();
		if(!analysis::isObjectType(type) || analysis::isUnion(type)) {
			add(res, Message(address, EC_TYPE_ILLEGAL_OBJECT_TYPE, format("Invalid parent type - not an object: %s", toString(*type)), Message::ERROR));
		}

		return res;
	}


	OptionalMessageList FreeTagTypeReferencesCheck::visitNode(const NodeAddress& address) {
		OptionalMessageList res;

		using TagTypeRefs = std::vector<TagTypeReferenceAddress>;

		// an internal reference collector
		struct FreeTagTypeCollector : public IRVisitor<TagTypeRefs> {

			std::map<NodePtr,TagTypeRefs> cache;

			FreeTagTypeCollector() : IRVisitor<TagTypeRefs>(true) {}

			TagTypeRefs visit(const NodePtr& cur) override {
				// check cache
				auto pos = cache.find(cur);
				if (pos != cache.end()) {
					return pos->second;
				}

				// dispatch & cache
				return cache[cur] = IRVisitor<TagTypeRefs>::visit(cur);
			}

			TagTypeRefs visitTagTypeReference(const TagTypeReferencePtr& ref) override {
				TagTypeRefs res;
				res.push_back(TagTypeReferenceAddress(ref));
				return res;
			}

			TagTypeRefs visitTagTypeDefinition(const TagTypeDefinitionPtr& def) override {
				// aggregate references of child nodes - filtered by definitions
				TagTypeRefs res;
				for(const auto& cur : TagTypeDefinitionAddress(def)) {
					RecordAddress recordAdr = cur->getRecord();
					for(const auto& ref : visit(recordAdr)) {
						if (!def->getDefinitionOf(ref)) {
							res.push_back(concat(recordAdr, ref));
						}
					}
				}
				return res;
			}

			TagTypeRefs visitTagType(const TagTypePtr& type) override {
				// skipping the tag-type reference in the tag type
				TagTypeRefs res;
				TagTypeDefinitionAddress defAddr = TagTypeAddress(type)->getDefinition();
				for(const auto& cur : visit(defAddr)) {
					res.push_back(concat(defAddr, cur));
				}
				return res;
			}

			TagTypeRefs visitNode(const NodePtr& cur) override {
				// default: aggregate free references of children
				TagTypeRefs res;
				NodeAddress addr(cur);
				for(const auto& child : addr.getChildList()) {
					for(const auto& ref : visit(child)) {
						res.push_back(concat(child,ref));
					}
				}
				return res;
			}

		};

		// get free references
		auto free = FreeTagTypeCollector()(address);

		// check if there are free references
		if (free.empty()) return res;

		// correct address
		if (!address.isRoot()) {
			for(auto& cur : free) {
				cur = concat(address, cur);
			}
		}

		// add errors
		for(const auto& cur : free) {
			// in all other cases there is a free definition
			add(res, Message(cur, EC_TYPE_FREE_TAG_TYPE_REFERENCE, format("Free tag type reference %s found", *cur), Message::ERROR));
		}

		// done
		return res;
	}

	OptionalMessageList TagTypeFieldsCheck::visitTagType(const TagTypeAddress& address) {
		OptionalMessageList res;

		// check if enum type
		if(lang::isEnumType(address.getAddressedNode())) {
			auto fields = address.getAddressedNode()->getFields();
			//only check if non generic enum type?!
			if(!fields[0]->getType().isa<TypeVariablePtr>()) {
				// enum definition has to be a generic type ptr
				// that has >= 1 type param and the name "enum_def<...>"
				auto enumDefType = fields[0]->getType().isa<GenericTypePtr>();
				if(enumDefType && enumDefType->getTypeParameter().size() &&
					(enumDefType->getName()->getValue().find("enum_def") != std::string::npos)) {
					//first field of enum_def has to be a string lit
					if(GenericTypePtr enumName = enumDefType->getTypeParameter(0).isa<GenericTypePtr>()) {
						if(enumName->getTypeParameter()->size()) {
							add(res, Message(address, EC_TYPE_MALFORMED_ENUM_TYPE_DEFINITION_NAME,
								format("Enum definition contains invalid name: ", *(enumDefType->getTypeParameter(0)), Message::ERROR)));
						}
					} else {
							add(res, Message(address, EC_TYPE_MALFORMED_ENUM_TYPE_DEFINITION_NAME,
								format("Enum definition contains invalid name: ", *(enumDefType->getTypeParameter(0)), Message::ERROR)));
					}
					//all following fields must be enum entries
					for(unsigned i=1; i<enumDefType->getTypeParameter()->size(); ++i) {
						if(!lang::EnumEntry::isEnumEntry(enumDefType->getTypeParameter(i))) {
							add(res, Message(address, EC_TYPE_MALFORMED_ENUM_ENTRY,
								format("Enum definition contains invalid enum entry: ", *(enumDefType->getTypeParameter(i)), Message::ERROR)));
						}
					}
				} else {
					add(res, Message(address, EC_TYPE_MALFORMED_ENUM_TYPE, format("Invalid enum type: ",
						*(address.getAddressedNode()), Message::ERROR)));
				}
			}
		}

		// check for duplicate field names
		utils::set::PointerSet<StringValuePtr> identifiers;
		auto fields = address.getAddressedNode()->getFields();
		for(auto field : fields) {
			auto id = field->getName();
			if(id->getValue().empty()) continue;
			if(identifiers.contains(id)) {
				add(res, Message(address, EC_TYPE_MALFORMED_TAG_TYPE, format("Tag type contains duplicate field name: ", *id), Message::ERROR));
			}
			identifiers.insert(id);
		}

		return res;
	}

	namespace {

		void checkMemberType(const TagTypeBindingAddress& address, const FunctionTypePtr& functionType, const enum FunctionKind expectedFunctionKind, const bool expectSameReturnType,
		                     OptionalMessageList& res, const enum ErrorCode errorCode, const std::string& msg) {
			NodeManager& mgr = address->getNodeManager();
			IRBuilder builder(mgr);

			auto params = functionType->getParameterTypeList();
			if (params.size() == 0 || !analysis::isRefType(params[0])) {
				//generic check should handle this case
				return;
			}

			// get the tag
			auto originalThisReference = lang::ReferenceType(params[0]);
			auto tag = address.as<TagTypeBindingPtr>()->getTag();
			TypePtr thisType = builder.refType(tag, originalThisReference.isConst(), originalThisReference.isVolatile());

			TypeList paramTypes;
			paramTypes.push_back(thisType);
			paramTypes.insert(paramTypes.end(), ++(params.begin()), params.end());

			auto expected = builder.functionType(paramTypes, expectSameReturnType ? functionType->getReturnType() : thisType, expectedFunctionKind);

			if (*expected != *functionType) {
				add(res, Message(address, errorCode, format("%s: %s - expected: %s", msg, *functionType, *expected), Message::ERROR));
			}
		}

	}

	OptionalMessageList ConstructorTypeCheck::visitTagTypeBinding(const TagTypeBindingAddress& address) {
		OptionalMessageList res;

		// iterate over all the constructors and check their types
		for (auto& constructor : address->getRecord()->getConstructors()) {
			checkMemberType(address, constructor.getAddressedNode().as<LambdaExprPtr>()->getFunctionType(), FK_CONSTRUCTOR, false, res, EC_TYPE_INVALID_CONSTRUCTOR_TYPE, "Invalid constructor type");
		}

		return res;
	}

	OptionalMessageList DuplicateConstructorTypeCheck::visitTagTypeBinding(const TagTypeBindingAddress& address) {
		OptionalMessageList res;

		NodeManager& mgr = address->getNodeManager();
		IRBuilder builder(mgr);

		std::set<FunctionTypePtr> constructorTypes;
		for (const auto& ctor : address->getRecord()->getConstructors()) {
			const auto& type = ctor.getAddressedNode().as<LambdaExprPtr>()->getFunctionType();
			auto inserted = constructorTypes.insert(type);

			if (!inserted.second) {
				add(res, Message(address, EC_TYPE_DUPLICATE_CONSTRUCTOR_TYPE, format("Duplicate constructor type: %s", *type), Message::ERROR));
			}
		}

		return res;
	}

	OptionalMessageList DestructorTypeCheck::visitTagTypeBinding(const TagTypeBindingAddress& address) {
		OptionalMessageList res;

		checkMemberType(address, address.getAddressedNode()->getRecord()->getDestructor().as<LambdaExprPtr>()->getFunctionType(), FK_DESTRUCTOR, false, res, EC_TYPE_INVALID_DESTRUCTOR_TYPE, "Invalid destructor type");

		return res;
	}

	OptionalMessageList MemberFunctionTypeCheck::visitTagTypeBinding(const TagTypeBindingAddress& address) {
		OptionalMessageList res;

		// iterate over all the member functions and check their type
		for (auto& memberFunction : address->getRecord()->getMemberFunctions()) {
			checkMemberType(address, memberFunction.getAddressedNode()->getImplementation().as<LambdaExprPtr>()->getFunctionType(), FK_MEMBER_FUNCTION, true, res, EC_TYPE_INVALID_MEMBER_FUNCTION_TYPE, "Invalid member function type");
		}

		// iterate over all the pure virtual member functions and check their type
		for (auto& memberFunction : address->getRecord()->getPureVirtualMemberFunctions()) {
			checkMemberType(address, memberFunction.getAddressedNode()->getType(), FK_MEMBER_FUNCTION, true, res, EC_TYPE_INVALID_MEMBER_FUNCTION_TYPE, "Invalid pure virtual member function type");
		}

		return res;
	}

	OptionalMessageList DuplicateMemberFunctionCheck::visitTagTypeBinding(const TagTypeBindingAddress& address) {
		OptionalMessageList res;

		NodeManager& mgr = address->getNodeManager();
		IRBuilder builder(mgr);

		std::map<std::string, std::set<FunctionTypePtr>> memberFunctionTypes;
		for (const auto& memberFunction : address->getRecord()->getMemberFunctions()) {
			const auto& type = memberFunction.getAddressedNode()->getImplementation().as<LambdaExprPtr>()->getFunctionType();
			const auto& name = memberFunction->getName()->getValue();
			auto inserted = memberFunctionTypes[name].insert(type);

			if (!inserted.second) {
				add(res, Message(address, EC_TYPE_DUPLICATE_MEMBER_FUNCTION, format("Duplicate member function type: %s for name %s", *type, name), Message::ERROR));
			}
		}
		for (const auto& memberFunction : address->getRecord()->getPureVirtualMemberFunctions()) {
			const auto& type = memberFunction.getAddressedNode()->getType();
			const auto& name = memberFunction->getName()->getValue();
			auto inserted = memberFunctionTypes[name].insert(type);

			if (!inserted.second) {
				add(res, Message(address, EC_TYPE_DUPLICATE_MEMBER_FUNCTION, format("Duplicate member function type: %s for name %s", *type, name), Message::ERROR));
			}
		}

		return res;
	}
	
	OptionalMessageList DuplicateMemberFieldCheck::visitFields(const FieldsAddress& address) {
		OptionalMessageList res;

		std::set<std::string> fieldNames;
		for (const auto& field : address->getFields()) {
			const auto& fieldName = field->getName()->getValue();
			if (fieldName.empty()) {
				add(res, Message(address, EC_TYPE_INVALID_IDENTIFIER, format("Empty member field name for type: %s", *field->getType()), Message::ERROR));
				// as we do not want to insert an empty string into the set
				continue;
			}
			// store the name of the field in the unique set (no duplicates)
			auto inserted = fieldNames.insert(fieldName);
			if(!inserted.second) {
				add(res, Message(address, EC_TYPE_DUPLICATE_MEMBER_FIELD, format("Duplicate member field type: %s for name %s", *field->getType(), fieldName), Message::ERROR));
			}
		}
		return res;
	}

	OptionalMessageList CallExprTypeCheck::visitCallExpr(const CallExprAddress& address) {
		NodeManager& manager = address->getNodeManager();
		OptionalMessageList res;

		// obtain function type ...
		TypePtr funType = address->getFunctionExpr()->getType();
		assert_eq(address->getFunctionExpr()->getType()->getNodeType(), NT_FunctionType) << "Illegal function expression!";

		const FunctionTypePtr& functionType = funType.as<FunctionTypePtr>();
		const TypeList& parameterTypes = functionType->getParameterTypes()->getTypes();
		const TypePtr& returnType = functionType->getReturnType();

		// Obtain argument type
		TypeList argumentTypes;
		::transform(address.as<CallExprPtr>()->getArguments(), back_inserter(argumentTypes), [](const ExpressionPtr& cur) { return cur->getType(); });

		// 1) check number of arguments
		int numParameter = parameterTypes.size();
		int numArguments = argumentTypes.size();
		if(numArguments != numParameter) {
			add(res, Message(address, EC_TYPE_INVALID_NUMBER_OF_ARGUMENTS,
			                 format("Wrong number of arguments \nexpected: %d\n", numParameter) + format("actual: %d\n ", numArguments)
			                     + format("function type: \n\t%s", *functionType),
			                 Message::ERROR));
			return res;
		}

		// 2) check types of arguments => using variable deduction
		auto substitution = types::getTypeVariableInstantiation(manager, address);

		if(!substitution) {
			TupleTypePtr argumentTuple = TupleType::get(manager, argumentTypes);
			TupleTypePtr parameterTuple = TupleType::get(manager, parameterTypes);
			add(res, Message(address, EC_TYPE_INVALID_ARGUMENT_TYPE, format("Invalid argument type(s) \nexpected: \n\t%s\n", *parameterTuple)
			                                                             + format("actual: \n\t%s\n", *argumentTuple)
			                                                             + format("function type: \n\t%s", *functionType),
			                 Message::ERROR));
			return res;
		}

		// 3) check return type - which has to be matched with modified function return value.
		TypePtr retType = substitution->applyTo(returnType);
		TypePtr resType = address->getType();

		if(!core::types::isSubTypeOf(retType, resType)) {
			add(res, Message(address, EC_TYPE_INVALID_RETURN_TYPE,
			                 format("Invalid result type of call expression \nexpected: \n\t%s \nactual: \n\t%s \nfunction type: \n\t%s",
			                        *retType, *resType, *functionType),
			                 Message::ERROR));
			return res;
		}
		return res;
	}

	OptionalMessageList BindExprTypeCheck::visitBindExpr(const BindExprAddress& address) {
		NodeManager& manager = address->getNodeManager();
		OptionalMessageList res;

		// recreate type
		auto extractType = [](const VariablePtr& var) { return var->getType(); };

		TypeList param;
		::transform(address.getAddressedNode()->getParameters()->getElements(), back_inserter(param), extractType);

		TypePtr isType = address->getType();
		TypePtr result = address->getCall()->getType();

		FunctionTypePtr funType = FunctionType::get(manager, param, result, FK_CLOSURE);
		if(*funType != *isType) {
			add(res, Message(address, EC_TYPE_INVALID_FUNCTION_TYPE,
			                 format("Invalid type of bind expression - expected: \n%s, actual: \n%s", *funType, *isType),
			                 Message::ERROR));
		}
		return res;
	}

	OptionalMessageList ExternalFunctionTypeCheck::visitLiteral(const LiteralAddress& address) {
		OptionalMessageList res;

		// only important for function types
		core::TypePtr type = address->getType();
		if(type->getNodeType() != core::NT_FunctionType) { return res; }

		core::FunctionTypePtr funType = static_pointer_cast<const core::FunctionType>(type);
		if(funType->isClosure()) {
			add(res, Message(address, EC_TYPE_INVALID_FUNCTION_TYPE, format("External literals must not be closure types!"), Message::ERROR));
		}
		return res;
	}


	OptionalMessageList ReturnTypeCheck::visitLambda(const LambdaAddress& address) {
		OptionalMessageList res;

		// obtain return type of lambda
		const TypePtr& returnType = address->getType()->getReturnType();

		// search for all return statements and check type
		visitDepthFirstPrunable(address, [&](const NodeAddress& cur) -> bool {

			// check whether it is a a return statement
			if(cur->getNodeType() != NT_ReturnStmt) {
				// abort if this node is a expression or type
				NodeCategory category = cur->getNodeCategory();
				return (category == NC_Type || category == NC_Expression);
			}

			const ReturnStmtAddress& returnStmt = static_address_cast<const ReturnStmt>(cur);
			const TypePtr& actualType = returnStmt->getReturnExpr()->getType();
			if(!core::types::isSubTypeOf(actualType, returnType)) {
				add(res, Message(cur, EC_TYPE_INVALID_RETURN_VALUE_TYPE,
				                 format("Invalid type of return value \nexpected: \n\t%s\n actual: \n\t%s", toString(*returnType), toString(*actualType)),
				                 Message::ERROR));
			}

			return true;
		}, false);


		// EC_TYPE_MISSING_RETURN_STMT,

		return res;
	}

	OptionalMessageList LambdaTypeCheck::visitLambdaExpr(const LambdaExprAddress& address) {
		OptionalMessageList res;

		// get lambda expression
		LambdaExprPtr lambda = address.getAddressedNode();

		// check that rec-lambda variable does exist within definitions
		if(!lambda->getDefinition()->getDefinitionOf(lambda->getReference())) {
			add(res, Message(address, EC_TYPE_INVALID_LAMBDA_EXPR_NO_SUCH_DEFINITION,
			                 format("No definition found for rec-lambda variable %s", toString(*lambda->getReference())), Message::ERROR));

			// no further checks useful
			return res;
		}


		// check type of lambda expression compared to rec-lambda variable type
		TypePtr is = lambda->getType();
		TypePtr should = lambda->getReference()->getType();
		if(*is != *should) {
			add(res, Message(address, EC_TYPE_INVALID_LAMBDA_EXPR_TYPE,
			                 format("Lambda-Expression Type does not match rec-lambda Variable Type - is: %s, should: %s", toString(*is), toString(*should)),
			                 Message::ERROR));
		}

		// check type of recursive variable
		assert_true(lambda->getDefinition()->getDefinitionOf(lambda->getReference()));
		is = lambda->getReference()->getType();
		should = lambda->getDefinition()->getDefinitionOf(lambda->getReference())->getType();
		if(*is != *should) {
			add(res, Message(address, EC_TYPE_INVALID_LAMBDA_REC_VAR_TYPE,
			                 format("Type of recursive lambda variable %s does not fit type of lambda - is: %s, should: %s", toString(*lambda->getReference()),
			                        toString(*is), toString(*should)),
			                 Message::ERROR));
		}

		// check that all parameters are references
		bool parametersOK = true;
		for(const auto& cur : address->getLambda()->getParameterList()) {
			if (!lang::isReference(cur)) {
				add(res, Message(cur, EC_TYPE_INVALID_LAMBDA_PARAMETER_TYPE,
						format("Invalid parameter type: %s -- all lambda parameters need to be references.", toString(*cur->getType()))));
				parametersOK = false;
			}
		}

		// stop here if not all parameters are references
		if (!parametersOK) return res;

		// check type of lambda
		IRBuilder builder(lambda->getNodeManager());
		FunctionTypePtr funTypeIs = lambda->getLambda()->getType();
		FunctionTypePtr funTypeShould =
		    builder.functionType(::transform(lambda->getLambda()->getParameterList(), [](const VariablePtr& cur) { return analysis::getReferencedType(cur->getType()); }),
		                         funTypeIs->getReturnType(), funTypeIs->getKind());
		if(*funTypeIs != *funTypeShould) {
			add(res, Message(address, EC_TYPE_INVALID_LAMBDA_TYPE, format("Invalid type of lambda definition for variable %s - is: %s, should %s",
			                                                              toString(*lambda->getReference()), toString(*funTypeIs), toString(*funTypeShould)),
			                 Message::ERROR));
		}

		return res;
	}


	OptionalMessageList ArrayTypeCheck::visitNode(const NodeAddress& address) {
		OptionalMessageList res;

		// filter out everything which is not a type or expression
		NodeCategory cat = address->getNodeCategory();
		if(cat != NC_Expression && cat != NC_Type) {
			return res; // this test is only covering expressions and types
		}

		// check expressions (must not be arrays except within very few cases)
		if(cat == NC_Expression) {
			ExpressionPtr expr = address.as<ExpressionPtr>();
			if(lang::isUnknownSizedArray(expr)) {

				// no value instantiation allowed for unknown size arrays
				add(res, Message(address, EC_TYPE_INVALID_ARRAY_VALUE,
				                 format("Invalid instantiation of array value of type %s! Arrays must not be accessed by value, only by reference.",
				                        toString(*address)),
				                 Message::ERROR));
				return res;
			}
		}

		// the rest are just limitations on types
		if(cat != NC_Type) { return res; }

		// union, ref and array types are fine


		// check composition of struct types
		if(StructPtr structType = analysis::isStruct(address)) {
			auto fields = structType->getFields();
			if(fields.empty()) { return res; }
			for(const auto& field : fields) {
				if(lang::isVariableSizedArray(field->getType())) {
					add(res, Message(address, EC_TYPE_INVALID_ARRAY_CONTEXT,
									 "Variable sized array not allowed within struct types.", Message::ERROR));
				}
			}
			for(auto it = fields.begin(); it != fields.end() - 1; ++it) {
				if(lang::isUnknownSizedArray(it->getType())) {
					add(res, Message(address, EC_TYPE_INVALID_ARRAY_CONTEXT,
					                 "Unknown sized data structure has to be the last component of enclosing struct type.", Message::ERROR));
				}
			}
			return res;
		}

		// check tuple types
		if(address->getNodeType() == NT_TupleType) {
			TupleTypePtr tupleType = address.as<TupleTypePtr>();
			if(tupleType.empty()) { return res; }
			for(auto it = tupleType.begin(); it != tupleType.end(); ++it) {
				if(lang::isArray(*it) && !lang::isFixedSizedArray(*it)) {
					add(res, Message(address, EC_TYPE_INVALID_ARRAY_CONTEXT,
					                 "Arrays within tuple types need to be fixed-size.", Message::ERROR));
				}
			}
			return res;
		}

		// no issues identified
		return res;
	}

	OptionalMessageList GenericOpsCheck::visitCallExpr(const CallExprAddress& address) {
		// get as pointer
		CallExprPtr call = address;
		auto& base = call->getNodeManager().getLangBasic();

		OptionalMessageList res;

		auto fun = call.as<CallExprPtr>()->getFunctionExpr();

		// only interested in generic operators
		if(!fun.isa<LiteralPtr>() || !base.isGenOp(fun)) { return res; }

		// arguments need to be arithmetic types or function types
		for(auto arg : call) {
			auto type = arg->getType();
			if(!type.isa<TypeVariablePtr>() && !base.isScalarType(type) && !type.isa<FunctionTypePtr>() && !core::lang::isEnumType(type)) {
				add(res, Message(address, EC_TYPE_INVALID_GENERIC_OPERATOR_APPLICATION,
				                 format("Generic operators must only be applied on arithmetic types - found: %s", toString(*type)), Message::ERROR));
			}
		}

		// done
		return res;
	}


	OptionalMessageList DeclarationStmtTypeCheck::visitDeclarationStmt(const DeclarationStmtAddress& address) {
		OptionalMessageList res;

		DeclarationStmtPtr declaration = address.getAddressedNode();

		// just test whether same type is on both sides
		TypePtr variableType = declaration->getVariable()->getType();
		TypePtr initType = declaration->getInitialization()->getType();

		if(!types::isSubTypeOf(initType, variableType)) {
			add(res, Message(address, EC_TYPE_INVALID_INITIALIZATION_EXPR, format("Invalid type of initial value - expected: \n%s, actual: \n%s",
			                                                                      *variableType, *initType),
			                 Message::ERROR));
		}
		return res;
	}

	OptionalMessageList IfConditionTypeCheck::visitIfStmt(const IfStmtAddress& address) {
		const NodeManager& manager = address->getNodeManager();
		OptionalMessageList res;

		TypePtr conditionType = address->getCondition()->getType();

		if(!manager.getLangBasic().isBool(conditionType)) {
			add(res,
			    Message(address, EC_TYPE_INVALID_CONDITION_EXPR, format("Invalid type of condition expression - expected: \n%s, actual: \n%s",
			                                                            *manager.getLangBasic().getBool(), *conditionType),
			            Message::ERROR));
		}
		return res;
	}

	OptionalMessageList ForStmtTypeCheck::visitForStmt(const ForStmtAddress& address) {
		OptionalMessageList res;

		core::ForStmtPtr node = address.getAddressedNode();
		auto& basic = node->getNodeManager().getLangBasic();

		// get type of iterator
		core::TypePtr iteratorType = node->getIterator()->getType();

		// check iterator type
		if(!basic.isInt(iteratorType)) {
			add(res,
			    Message(address, EC_TYPE_INVALID_ITERATOR_TYPE,
			            format("Invalid type of iterator variable - expected: some integral, actual: %s\n", *iteratorType), Message::ERROR));
			return res;
		}

		if(!types::isSubTypeOf(node->getEnd().getType(), iteratorType)) {
			add(res, Message(address, EC_TYPE_INVALID_BOUNDARY_TYPE, format("Invalid type of upper loop boundary - expected: %s, actual: %s\n",
			                                                                *iteratorType, *node->getEnd().getType()),
			                 Message::ERROR));
		}

		if(!types::isSubTypeOf(node->getStep().getType(), iteratorType)) {
			add(res, Message(address, EC_TYPE_INVALID_BOUNDARY_TYPE, format("Invalid type of step size - expected: %s, actual: %s\n",
			                                                                *iteratorType, *node->getStep().getType()),
			                 Message::ERROR));
		}

		return res;
	}

	OptionalMessageList WhileConditionTypeCheck::visitWhileStmt(const WhileStmtAddress& address) {
		const NodeManager& manager = address->getNodeManager();
		OptionalMessageList res;

		TypePtr conditionType = address->getCondition()->getType();
		if(!manager.getLangBasic().isBool(conditionType)) {
			add(res,
			    Message(address, EC_TYPE_INVALID_CONDITION_EXPR, format("Invalid type of condition expression - expected: \n%s, actual: \n%s",
			                                                            *manager.getLangBasic().getBool(), *conditionType),
			            Message::ERROR));
		}
		return res;
	}


	OptionalMessageList SwitchExpressionTypeCheck::visitSwitchStmt(const SwitchStmtAddress& address) {
		const NodeManager& manager = address->getNodeManager();

		OptionalMessageList res;
		TypePtr switchType = address->getSwitchExpr()->getType();
		if(!manager.getLangBasic().isInt(switchType)) {
			add(res,
			    Message(address, EC_TYPE_INVALID_SWITCH_EXPR,
			            format("Invalid type of switch expression - expected: integral type, actual: \n%s", *switchType), Message::ERROR));
		}
		return res;
	}


	OptionalMessageList StructExprTypeCheck::visitStructExpr(const StructExprAddress& address) {
		OptionalMessageList res;

		// extract type
		core::TypePtr type = address.getAddressedNode()->getType();

		// get struct type
		core::StructPtr structType = analysis::isStruct(type);

		// check whether it is a struct type
		if(!structType) {
			add(res, Message(address, EC_TYPE_INVALID_TYPE_OF_STRUCT_EXPR, format("Invalid type of struct-expression - type: \n%s", *type),
			                 Message::ERROR));
			return res;
		}

		// check type of values within struct expression
		for_each(address.getAddressedNode()->getMembers()->getNamedValues(), [&](const NamedValuePtr& cur) {
			core::TypePtr requiredType = structType->getFieldType(cur->getName());
			core::TypePtr isType = cur->getValue()->getType();
			if(!requiredType) {
				add(res, Message(address, EC_TYPE_INVALID_INITIALIZATION_EXPR, format("No member %s in struct type %s", cur->getName(), *structType),
					             Message::ERROR));
			} else if(!types::isSubTypeOf(isType, requiredType)) {
				add(res,
					Message(address, EC_TYPE_INVALID_INITIALIZATION_EXPR,
					        format("Invalid type of struct-member initalization - expected type: \n%s, actual: \n%s", *requiredType, *isType), Message::ERROR));
			}
		});

		return res;
	}


	namespace {


		OptionalMessageList checkMemberAccess(const NodeAddress& address, const ExpressionPtr& structExpr, const StringValuePtr& identifier,
		                                      const TypePtr& elementType, bool isRefVersion) {
			OptionalMessageList res;

			// check whether it is a struct at all
			TypePtr exprType = structExpr->getType();
			if(isRefVersion) {
				if(analysis::isRefType(exprType)) {
					// extract element type
					exprType = analysis::getReferencedType(exprType);
				} else {
					// invalid argument => handled by argument check
					return res;
				}
			}


			// Accessing an element from anything else than a tag type
			// we allow; since we have no way to check the consistency of
			// the requested element, everything is fine
			auto tagType = exprType.isa<TagTypePtr>();
			if(!tagType) return res;		// all fine

			// resolve recursive types
			if (tagType->isRecursive()) {
				tagType = tagType->peel();
			}

			// get member type
			const TypePtr& resultType = tagType->getFieldType(identifier);
			if(!resultType) {
				add(res, Message(address, EC_TYPE_NO_SUCH_MEMBER,
				                 format("No member '%s' within record type '%s'", *identifier, *tagType),
				                 Message::ERROR));
				return res;
			}

			// handle anonymous fields
			if(identifier->getValue().empty()) {
				// iterate through fields to find correct
				for(auto field : tagType->getFields()) {
					if(!field->getName()->getValue().empty()) continue;
					if(field->getType() == elementType) return res;
				}
				add(res, Message(address, EC_TYPE_INVALID_TYPE_OF_MEMBER, format("No anonymous member of type '%s' in record", *elementType), Message::ERROR));
				return res;
			}

			// check for correct member type
			if(!core::analysis::equalTypes(elementType, resultType)) {
				add(res, Message(address, EC_TYPE_INVALID_TYPE_OF_MEMBER,
					             format("Invalid type of extracted member '%s' - expected '%s'", *resultType, *elementType), Message::ERROR));
				return res;
			}

			// no problems found
			return res;
		}
	}

	OptionalMessageList MemberAccessElementTypeCheck::visitCallExpr(const CallExprAddress& address) {
		NodeManager& manager = address->getNodeManager();
		OptionalMessageList res;

		// check whether it is a call to the member access expression
		bool isMemberAccess = analysis::isCallOf(address.getAddressedNode(), manager.getLangBasic().getCompositeMemberAccess());
		bool isMemberReferencing = analysis::isCallOf(address.getAddressedNode(), manager.getLangExtension<lang::ReferenceExtension>().getRefMemberAccess());
		if(!isMemberAccess && !isMemberReferencing) {
			// no matching case
			return res;
		}

		if(address->getArguments().size() != 3) {
			// incorrect function usage => let function check provide errors
			return res;
		}

		// extract parameters
		const ExpressionPtr& structExpr = address->getArgument(0);
		const ExpressionPtr& identifierExpr = address->getArgument(1);
		const TypePtr& elementType = address->getArgument(2)->getType();

		// check identifier literal
		if(identifierExpr->getNodeType() != NT_Literal) {
			add(res, Message(address, EC_TYPE_INVALID_IDENTIFIER,
			                 format("Invalid identifier expression \n%s - not a constant.", *identifierExpr), Message::ERROR));
			return res;
		}

		// check type literal
		TypePtr resultType;
		if(GenericTypePtr genType = dynamic_pointer_cast<const GenericType>(elementType)) {
			if(genType->getName()->getValue() != "type" || genType->getTypeParameter()->size() != 1) {
				// invalid argument => leaf issues to argument type checker
				return res;
			}

			// retrieve type
			resultType = genType->getTypeParameter()[0];

		} else {
			// invalid arguments => argument type checker will handle it
			return res;
		}

		// extract the value of the literal
		const LiteralPtr& identifierLiteral = static_pointer_cast<const Literal>(identifierExpr);
		const StringValuePtr memberName = identifierLiteral->getValue();

		// use common check routine
		return checkMemberAccess(address, structExpr, memberName, resultType, isMemberReferencing);
	}


	namespace {


		OptionalMessageList checkTupleAccess(const NodeAddress& address, const ExpressionPtr& tupleExpr, unsigned index, const TypePtr& elementType,
		                                     bool isRefVersion) {
			OptionalMessageList res;

			// check whether it is a tuple at all
			TypePtr exprType = tupleExpr->getType();
			if(isRefVersion) {
				if(analysis::isRefType(exprType)) {
					// extract element type
					exprType = analysis::getReferencedType(exprType);
				} else {
					// invalid argument => handled by argument check
					return res;
				}
			}

			// check whether it is a tuple type
			const TupleTypePtr tupleType = dynamic_pointer_cast<const TupleType>(exprType);
			if(!tupleType) {
				add(res, Message(address, EC_TYPE_ACCESSING_MEMBER_OF_NON_TUPLE_TYPE, format("Cannot access element #%d of non-tuple type \n%s of type \n%s",
				                                                                             index, *tupleExpr, *exprType),
				                 Message::ERROR));
				return res;
			}

			// get element type
			unsigned numElements = tupleType->getElements().size();
			if(numElements <= index) {
				add(res, Message(address, EC_TYPE_NO_SUCH_MEMBER,
				                 format("No element with index %d within tuple type \n%s", index, *tupleType), Message::ERROR));
				return res;
			}

			const TypePtr& resultType = tupleType->getElement(index);

			// check for correct element type
			if(elementType != resultType) {
				add(res, Message(address, EC_TYPE_INVALID_TYPE_OF_MEMBER,
				                 format("Invalid type of extracted member \n%s - expected \n%s", *resultType, *elementType),
				                 Message::ERROR));
				return res;
			}

			// no problems found
			return res;
		}
	}

	OptionalMessageList ComponentAccessTypeCheck::visitCallExpr(const CallExprAddress& address) {
		NodeManager& manager = address->getNodeManager();
		OptionalMessageList res;

		// check whether it is a call to the tuple access expression
		bool isMemberAccess = analysis::isCallOf(address.getAddressedNode(), manager.getLangBasic().getTupleMemberAccess());
		bool isMemberReferencing = analysis::isCallOf(address.getAddressedNode(), manager.getLangExtension<lang::ReferenceExtension>().getRefComponentAccess());
		if(!isMemberAccess && !isMemberReferencing) {
			// no matching case
			return res;
		}

		if(address->getArguments().size() != 3) {
			// incorrect function usage => let function check provide errors
			return res;
		}

		// extract parameters
		const ExpressionPtr& tupleExpr = address->getArgument(0);
		ExpressionPtr indexExpr = address->getArgument(1);
		const TypePtr& elementType = address->getArgument(2)->getType();

		// check index literal
		while(indexExpr->getNodeType() == NT_CastExpr) { // TODO: remove this when removing casts
			indexExpr = static_pointer_cast<core::CastExprPtr>(indexExpr)->getSubExpression();
		}
		if(indexExpr->getNodeType() != NT_Literal) {
			add(res, Message(address, EC_TYPE_INVALID_TUPLE_INDEX, format("Invalid index expression \n%s - not a constant.", *indexExpr),
			                 Message::ERROR));
			return res;
		}

		// check type literal
		TypePtr resultType;
		if(GenericTypePtr genType = dynamic_pointer_cast<const GenericType>(elementType)) {
			if(genType->getName()->getValue() != "type" || genType->getTypeParameter()->size() != 1) {
				// invalid argument => leaf issues to argument type checker
				return res;
			}

			// retrieve type
			resultType = genType->getTypeParameter()[0];

		} else {
			// invalid arguments => argument type checker will handle it
			return res;
		}

		// extract the value of the literal
		const LiteralPtr& indexLiteral = static_pointer_cast<const Literal>(indexExpr);
		unsigned index = utils::numeric_cast<unsigned>(indexLiteral->getValue()->getValue());

		// use common check routine
		return checkTupleAccess(address, tupleExpr, index, resultType, isMemberReferencing);
	}


	OptionalMessageList BuiltInLiteralCheck::visitLiteral(const LiteralAddress& address) {
		OptionalMessageList res;

		// check whether it is a build-in literal
		const NodeManager& manager = address->getNodeManager();

		// obtain literal
		try {
			LiteralPtr buildIn = manager.getLangBasic().getLiteral(address->getValue()->getValue());

			// check whether used one is special case of build-in version
			if(*buildIn->getType() != *address->getType()) {
				add(res, Message(address, EC_TYPE_INVALID_TYPE_OF_LITERAL,
				                 format("Deviating type of build in literal \n%s - expected: \n%s, actual: \n%s", address->getValue()->getValue(),
				                        *buildIn->getType(), *address->getType()),
				                 Message::WARNING));
			}

		} catch(const lang::LiteralNotFoundException& lnfe) {
			// no such literal => all fine
		}

		return res;
	}

	namespace {

		unsigned getNumRefs(const TypePtr& type) {
			unsigned count = 0;
			TypePtr cur = type;
			while(analysis::isRefType(cur)) {
				count++;
				cur = analysis::getReferencedType(cur);
			}
			return count;
		}
	}

	OptionalMessageList RefCastCheck::visitCastExpr(const CastExprAddress& address) {
		OptionalMessageList res;

		// check whether it is a build-in literal
		TypePtr src = address->getSubExpression()->getType();
		TypePtr trg = address->getType();
		unsigned srcCount = getNumRefs(src);
		unsigned trgCount = getNumRefs(trg);

		if(srcCount > trgCount) {
			add(res, Message(address, EC_TYPE_REF_TO_NON_REF_CAST,
			                 format("Casting reference type %s to non-reference type %s", *src, *trg), Message::ERROR));
		}

		if(srcCount < trgCount) {
			add(res, Message(address, EC_TYPE_NON_REF_TO_REF_CAST,
			                 format("Casting non-reference type %s to reference type %s", *src, *trg), Message::ERROR));
		}

		return res;
	}

	namespace {

		bool isPrimitiveType(const TypePtr& type) {
			auto& basic = type->getNodeManager().getLangBasic();
			return basic.isChar(type) || basic.isBool(type) || basic.isScalarType(type);
		}

		bool isValidCast(const TypePtr& src, const TypePtr& trg) {
			// casting a type to itself is always allowed
			if(*src == *trg) { return true; }

			// allow cast to generic
			if(trg->getNodeType() == NT_TypeVariable) { return true; }

			// casts between integer values or reals are allowed
			if(isPrimitiveType(src) && isPrimitiveType(trg)) {
				return true; // this is allowed
			}

			// allow casts between recursive version and unrolled version
			TagTypePtr ts = src.isa<TagTypePtr>();
			TagTypePtr tt = trg.isa<TagTypePtr>();
			if (ts && tt) {
				if (ts->isRecursive() && !tt->isRecursive()) {
					return isValidCast(ts->peel(), tt);
				}
				if (!ts->isRecursive() && tt->isRecursive()) {
					return isValidCast(ts, tt->peel());
				}
			}

			// we also allow casts between references
			if(analysis::isRefType(src) && analysis::isRefType(trg)) {
				// check whether cast between target types is valid
				auto srcType = analysis::getReferencedType(src);
				auto trgType = analysis::getReferencedType(trg);

				if(analysis::isRefType(srcType) || analysis::isRefType(trg)) { return isValidCast(srcType, trgType); }

				// this is a
				return true;
			}

			// also allow function pointers to be casted to different type function pointers
			if(src->getNodeType() == NT_FunctionType && trg->getNodeType() == NT_FunctionType) { return true; }

			// everything else is invalid
			return false;
		}
	}

	OptionalMessageList IllegalNumCastCheck::visitCallExpr(const CallExprAddress& callExpr) {
		OptionalMessageList res;

		auto& mgr = callExpr->getNodeManager();
		auto& basic = mgr.getLangBasic();

		//skip all calls which aren't NumericCasts
		if (!analysis::isCallOf(callExpr.getAddressedNode(), basic.getNumericCast())) {
			return res;
		}

		// check number of parameters
		if (callExpr->size() != 2) {
			return res; // => will be handled by general call parameter check
		}

		// get source and target types
		TypePtr srcType = callExpr[0]->getType();
		TypePtr trgType = callExpr[1]->getType();

		// check whether the second type is a type literal
		if (!analysis::isTypeLiteralType(trgType)) {
			return res; // => will be handled by general call parameter check
		}

		// extract actual target type
		trgType = analysis::getRepresentedType(trgType);

		// create a validity check for the argument types
		auto isValidNumericType = [&](const TypePtr& type) {
			return type.isa<TypeVariablePtr>() || basic.isNumeric(type) || core::lang::isEnumType(type);
		};

		//check expression type
		if (!isValidNumericType(srcType)) {
			add(res, Message(callExpr[0], EC_SEMANTIC_ILLEGAL_NUM_CAST, format("given source value is not of a numeric type (%s).", *srcType), Message::ERROR));
		}

		//as well as the target type
		if (!isValidNumericType(trgType)) {
			add(res, Message(callExpr[1], EC_SEMANTIC_ILLEGAL_NUM_CAST, format("given target type is not a numeric type  (%s).", *trgType), Message::ERROR));
		}

		return res;
	}

	OptionalMessageList IllegalNumTypeToIntCheck::visitCallExpr(const CallExprAddress& callExpr) {
		OptionalMessageList res;

		auto& mgr = callExpr->getNodeManager();
		auto& basic = mgr.getLangBasic();

		//skip all calls which aren't numTypeToInt
		if (!analysis::isCallOf(callExpr.getAddressedNode(), basic.getNumTypeToInt())) {
			return res;
		}

		// check number of parameters
		if (callExpr->size() != 1) {
			return res; // => will be handled by general call parameter check
		}

		// get source type
		TypePtr srcType = callExpr[0]->getType();

		// check whether type is a type literal
		if (!analysis::isTypeLiteralType(srcType)) {
			return res; // => will be handled by general call parameter check
		}

		// extract actual source type
		srcType = analysis::getRepresentedType(srcType);

		// create a validity check for the argument types
		auto isValidNumericType = [&](const TypePtr& type) {
			return type.isa<TypeVariablePtr>() || basic.isNumeric(type);
		};

		if (!isValidNumericType(srcType)) {
			add(res, Message(callExpr[0], EC_SEMANTIC_ILLEGAL_NUM_TYPE_TO_INT, format("given source type is not a numeric type  (%s).", *srcType), Message::ERROR));
		}

		return res;
	}

	OptionalMessageList RefOfFunCastCheck::visitCallExpr(const CallExprAddress& callExpr) {
		OptionalMessageList res;

		auto& mgr = callExpr->getNodeManager();
		auto& refExt = mgr.getLangExtension<lang::ReferenceExtension>();
		auto& ptrExt = mgr.getLangExtension<lang::PointerExtension>();


		// check if CallExpr is ref_of_func
		if (!(refExt.isCallOfRefOfFunction(callExpr) || ptrExt.isCallOfPtrOfFunction(callExpr)))
			return res;

		// type check will be performed somewhrere else
		if  (callExpr->size() != 1)
			return res;

		auto argumentType = callExpr[0]->getType();

		if (argumentType.isa<TypeVariablePtr>()) {
			return res;		// this might still be a function
		}

		auto funType = argumentType.isa<FunctionTypePtr>();
		if (!funType || !funType->isPlain()) {
			add(res, Message(callExpr[0], EC_SEMANTIC_ILLEGAL_REF_TO_FUN_CAST,
							 format("this is a illegal ref_to_fun() cast!"), Message::ERROR));
			return res;
		}

		return res;
	}

	OptionalMessageList CastCheck::visitCastExpr(const CastExprAddress& address) {
		OptionalMessageList res;

		TypePtr src = address->getSubExpression()->getType();
		TypePtr trg = address->getType();

		// check whether cast is safe
		if(isValidCast(src, trg)) { return res; }

		// report an error
		add(res, Message(address, EC_TYPE_ILLEGAL_CAST, format("Casting between incompatible types %s and %s", toString(src), toString(trg)), Message::ERROR));

		return res;
	}

	OptionalMessageList GenericZeroCheck::visitCallExpr(const CallExprAddress& address) {
		auto& base = address->getNodeManager().getLangBasic();

		OptionalMessageList res;

		auto call = address.as<CallExprPtr>();
		auto type = call->getType();

		// if the result type is a generic type everything is fine
		if(!lang::isBuiltIn(type) && type.isa<GenericTypePtr>()) { return res; }

		// only interested in get-zero expressions
		if(!core::analysis::isCallOf(call, base.getZero())) { return res; }

		// now we have a problem
		add(res, Message(address, EC_TYPE_ILLEGAL_GENERIC_ZERO_TYPE, format("Can not create generic zero element for type %s", toString(*call->getType())),
		                 Message::ERROR));

		return res;
	}


} // end namespace check
} // end namespace core
} // end namespace insieme
