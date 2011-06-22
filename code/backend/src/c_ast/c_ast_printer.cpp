/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
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

#include "insieme/backend/c_ast/c_ast_printer.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/string_utils.h"
#include "insieme/utils/printable.h"

namespace insieme {
namespace backend {
namespace c_ast {

	namespace {

		class CPrinter;

		struct PrintWrapper : public utils::Printable {
			CPrinter& printer;
			const NodePtr& node;
		public:
			PrintWrapper(CPrinter& printer, const NodePtr& node)
				: printer(printer), node(node) {};
			virtual std::ostream& printTo(std::ostream& out) const;
		};


		// the actual printer of the C AST
		class CPrinter {

			string indentStep;
			int indent;

		public:

			CPrinter(const string& indentStep = "    ")
				: indentStep(indentStep), indent(0) {}

			std::ostream& print(NodePtr node, std::ostream& out) {
				switch(node->getType()) {
					#define CONCRETE(name) case NT_ ## name: return print ## name (static_pointer_cast<name>(node), out);
					#include "insieme/backend/c_ast/c_nodes.def"
					#undef CONCRETE
				}
				assert(false && "Unknown C-Node type encountered!");
				return out;
			}

		private:

			PrintWrapper print(const NodePtr& node) {
				return PrintWrapper(*this, node);
			}

			ParameterPrinter printParam(const TypePtr& type, const IdentifierPtr& name) {
				return ParameterPrinter(type, name);
			}

			std::ostream& newLine(std::ostream& out) {
				return out << "\n" << times(indentStep, indent);
			}

			void incIndent() {
				indent++;
			}

			void decIndent() {
				indent--;
				assert(indent >= 0 && "Should never become < 0");
			}

			#define PRINT(name) std::ostream& print ## name (name ## Ptr node, std::ostream& out)

			PRINT(Identifier) {
				return out << node->name;
			}

			PRINT(Comment) {
				return out << "/* " <<  node->comment << " */";
			}

			PRINT(OpaqueCode) {
				return out << node->code;
			}

			PRINT(PrimitiveType) {
				return out << print(node->name);
			}

			PRINT(NamedType) {
				return out << print(node->name);
			}

			PRINT(PointerType) {
				return out << print(node->elementType) << "*";
			}

			PRINT(VectorType) {
				return out << print(node->elementType) << "[" << print(node->size) << "]";
			}

			PRINT(StructType) {
				return out << "struct " << print(node->name);
			}

			PRINT(UnionType) {
				return out << "union " << print(node->name);
			}

			PRINT(FunctionType) {
				return out << print(node->returnType) << "(" <<
						join(",", node->parameterTypes, [&](std::ostream& out, const TypePtr& cur) {
								out << print(cur);
				}) << ")";
			}

			PRINT(VarDecl) {
				// print a variable declaration
				out << print(node->var->type) << " " << print(node->var->name);

				if (!node->init) {
					return out;
				}

				return out << " = " << print(node->init);
			}

			PRINT(Compound) {
				out << "{";
				incIndent();
				newLine(out);

				std::size_t size = node->statements.size();
				for (std::size_t i=0; i<size; i++) {
					print(node->statements[i]);
					out << ";";
					if (i < size-1) newLine(out);
				}

				decIndent();
				newLine(out);
				out << "}";

				return out;
			}

			PRINT(If) {
				out << "if (";
				print(node->condition);
				out << ") ";
				print(node->thenStmt);
				if (node->elseStmt) {
					out << " else ";
					print(node->elseStmt);
				}
				return out;
			}

			PRINT(Switch) {
				out << "switch(";
				print(node->value);
				out << ") {";
				incIndent();
				newLine(out);

				std::size_t size = node->cases.size();
				for (std::size_t i=0; i<size; i++) {
					const std::pair<ExpressionPtr, StatementPtr>& cur = node->cases[i];
					out << "case ";
					print(cur.first);
					out << ": ";
					print(cur.second);
					if (i < size-1) newLine(out);
				}

				if (node->defaultBranch) {
					newLine(out);
					out << "default: ";
					print(node->defaultBranch);
				}

				decIndent();
				newLine(out);
				out << "}";
				return out;
			}

			PRINT(For) {
				out << "for (";
				print(node->init);
				out << "; ";
				print(node->check);
				out << "; ";
				print(node->step);
				out << ") ";
				print(node->body);
				return out;
			}

			PRINT(While) {
				out << "while (";
				print(node->condition);
				out << ") ";
				print(node->body);
				return out;
			}

			PRINT(Continue) {
				return out << "continue";
			}

			PRINT(Break) {
				return out << "break";
			}

			PRINT(Return) {
				return out << "return " << print(node->value);
			}

			PRINT(Literal) {
				return out << node->value;
			}

			PRINT(Variable) {
				return printIdentifier(node->name, out);
			}

			PRINT(Initializer) {
				out << "((";
				print(node->type);
				out << "){";

				std::size_t size = node->values.size();
				for(std::size_t i = 0; i<size; i++) {
					print(node->values[i]);
					if (i < size-1) out << ", ";
				}
				out << "})";

				return out;
			}

			PRINT(UnaryOperation) {

				// handle operations
				switch (node->operation) {
					case UnaryOperation::UnaryPlus: 	return out << "+" << print(node->operand);
					case UnaryOperation::UnaryMinus: 	return out << "-" << print(node->operand);
					case UnaryOperation::PrefixInc: 	return out << "++" << print(node->operand);
					case UnaryOperation::PrefixDec: 	return out << "--" << print(node->operand);
					case UnaryOperation::PostFixInc: 	return out << print(node->operand) << "++";
					case UnaryOperation::PostFixDec: 	return out << print(node->operand) << "--";
					case UnaryOperation::LogicNot: 		return out << "!" << print(node->operand);
					case UnaryOperation::BitwiseNot: 	return out << "~" << print(node->operand);
					case UnaryOperation::Indirection: 	return out << "*" << print(node->operand);
					case UnaryOperation::Reference: 	return out << "&" << print(node->operand);
					case UnaryOperation::SizeOf: 		return out << "sizeof(" << print(node->operand) << ")";
				}

				assert(false && "Invalid unary operation encountered!");
				return out;
			}

			PRINT(BinaryOperation) {

				string op = "";
				switch (node->operation) {
					case BinaryOperation::Assignment: 				op = " = "; break;
					case BinaryOperation::Additon: 					op = "+"; break;
					case BinaryOperation::Subtraction: 				op = "-"; break;
					case BinaryOperation::Multiplication: 			op = "*"; break;
					case BinaryOperation::Division: 				op = "/"; break;
					case BinaryOperation::Modulo: 					op = "%"; break;
					case BinaryOperation::Equal: 					op = "=="; break;
					case BinaryOperation::NotEqual: 				op = "!="; break;
					case BinaryOperation::GreaterThan: 				op = ">"; break;
					case BinaryOperation::LessThan: 				op = "<"; break;
					case BinaryOperation::GreaterOrEqual: 			op = ">="; break;
					case BinaryOperation::LessOrEqual: 				op = "<="; break;
					case BinaryOperation::LogicAnd: 				op = "&&"; break;
					case BinaryOperation::LogicOr: 					op = "||"; break;
					case BinaryOperation::BitwiseAnd: 				op = "&"; break;
					case BinaryOperation::BitwiseOr: 				op = "|"; break;
					case BinaryOperation::BitwiseXOr: 				op = "^"; break;
					case BinaryOperation::BitwiseLeftShift: 		op = "<<"; break;
					case BinaryOperation::BitwiseRightShift:		op = ">>"; break;
					case BinaryOperation::AdditionAssign: 			op = "+="; break;
					case BinaryOperation::SubtractionAssign: 		op = "-="; break;
					case BinaryOperation::MultiplicationAssign: 	op = "*="; break;
					case BinaryOperation::DivisionAssign: 			op = "/="; break;
					case BinaryOperation::ModuloAssign: 			op = "%="; break;
					case BinaryOperation::BitwiseAndAssign: 		op = "&="; break;
					case BinaryOperation::BitwiseOrAssign: 			op = "|="; break;
					case BinaryOperation::BitwiseXOrAssign: 		op = "^="; break;
					case BinaryOperation::BitwiseLeftShiftAssign: 	op = "<<="; break;
					case BinaryOperation::BitwiseRightShiftAssign: 	op = ">>="; break;
					case BinaryOperation::MemberAccess: 			op = "."; break;

					// special handling for subscript and cast
					case BinaryOperation::Subscript: print(node->operandA); out << "["; print(node->operandB); return out << "]";
					case BinaryOperation::Cast: return out << "(" << print(node->operandA) << ")" << print(node->operandB);

				}

				assert(op != "" && "Invalid binary operation encountered!");

				return out << print(node->operandA) << op << print(node->operandB);
			}

			PRINT(TernaryOperation) {

				switch (node->operation) {
				case TernaryOperation::TernaryCondition : {
					print(node->operandA);
					out << "?";
					print(node->operandB);
					out << ":";
					print(node->operandC);
					return out;
				}
				}

				assert(false && "Invalid ternary operation encountered!");
				return out;
			}

			PRINT(Call) {

				printIdentifier(node->function, out);
				out << "(";

				std::size_t size = node->arguments.size();
				for(std::size_t i = 0; i<size; i++) {
					print(node->arguments[i]);
					if (i < size-1) out << ", ";
				}

				return out << ")";
			}

			PRINT(Parentheses) {
				return out << "(" << print(node->expression) << ")";
			}

			PRINT(TypeDeclaration) {
				print(node->type);
				return out << ";\n";
			}

			PRINT(FunctionPrototype) {
				return out << "TODO";
			}

			PRINT(TypeDefinition) {

				bool explicitTypeDef = (bool)(node->name);

				// print prefix
				if (explicitTypeDef) {
					out << "typedef ";
				}

				// define type
				out << print(node->type);
				if (c_ast::NamedCompositeTypePtr composite = dynamic_pointer_cast<c_ast::NamedCompositeType>(node->type)) {
					out << " {\n    " << join(";\n    ", composite->elements,
							[&](std::ostream& out, const pair<c_ast::IdentifierPtr, c_ast::TypePtr>& cur) {
								out << printParam(cur.second, cur.first);
					}) << ";\n}";
				}

				// print name and finish
				if (explicitTypeDef) {
					out << " " << print(node->name);
				}
				return out << ";\n";
			}

			PRINT(Function) {
				return out << "TODO";
			}

			#undef PRINT

		};


		std::ostream& PrintWrapper::printTo(std::ostream& out) const {
			return printer.print(node, out);
		}

		struct TypeLevel {
			unsigned pointerCounter;
			vector<ExpressionPtr> subscripts;
			vector<TypePtr> parameters;
			TypeLevel() : pointerCounter(0) {}
		};

		typedef vector<TypeLevel> TypeNesting;
		typedef TypeNesting::const_iterator NestIterator;

		TypePtr computeNesting(TypeNesting& data, const TypePtr& type) {
			// check whether there is something to do
			if (type->getType() != NT_PointerType && type->getType() != NT_VectorType && type->getType() != NT_FunctionType) {
				return type;
			}

			TypePtr cur = type;
			TypeLevel res;

			// collect vector sizes
			while(cur->getType() == NT_VectorType) {
				VectorTypePtr vectorType = static_pointer_cast<VectorType>(cur);
				res.subscripts.push_back(vectorType->size);
				cur = vectorType->elementType;
			}

			// collect function parameters
			if (cur->getType() == NT_FunctionType) {
				// if vectors have already been processed => continue with next level
				if (!res.subscripts.empty()) {
					auto innermost = computeNesting(data, cur);
					data.push_back(res);
					return innermost;
				}

				FunctionTypePtr funType = static_pointer_cast<FunctionType>(cur);
				copy(funType->parameterTypes, std::back_inserter(res.parameters));

				cur = funType->returnType;
			}

			// count pointers
			while(cur->getType() == NT_PointerType) {
				res.pointerCounter++;
				cur = static_pointer_cast<PointerType>(cur)->elementType;
			}

			// resolve rest recursively
			auto innermost = computeNesting(data, cur);

			// add pair to result
			data.push_back(res);

			return innermost;
		}

		std::ostream& printTypeNest(std::ostream& out, NestIterator start, NestIterator end, const IdentifierPtr& name) {
			// terminal case ...
			if (start == end) {
				return out << " " << CPrint(name);
			}

			// print pointers ...
			const TypeLevel& cur = *start;
			out << times("*", cur.pointerCounter);

			++start;
			if (start != end) {
				out << "(";

				// print nested recursively
				printTypeNest(out, start, end, name);

				out << ")";
			} else {
				out << " " << CPrint(name);
			}

			// check whether one or the other is empty
			assert(!(!cur.parameters.empty() && !cur.subscripts.empty()) && "Only one component may be non-empty!");

			// print vector sizes
			out << join("", cur.subscripts, [&](std::ostream& out, const ExpressionPtr& cur) {
				out << "[" << CPrint(cur) << "]";
			});


			// print parameter list
			if (!cur.parameters.empty()) {
				out << "(" << join(",", cur.parameters, [&](std::ostream& out, const TypePtr& cur) {
					out << CPrint(cur);
				}) << ")";
			}

			return out;
		}


	}


	std::ostream& ParameterPrinter::printTo(std::ostream& out) const {

		// special handling for pointer to vectors
		TypeNesting nesting;
		TypePtr innermost = computeNesting(nesting, type);
		out << CPrint(innermost);
		return printTypeNest(out, nesting.begin(), nesting.end(), name);
	}


	std::ostream& CPrint::printTo(std::ostream& out) const {
		// use internal printer to generate code
		return CPrinter().print(fragment, out);
	}

	string toC(const NodePtr& node) {
		return ::toString(CPrint(node));
	}

} // end namespace c_ast
} // end namespace backend
} // end namespace insieme
