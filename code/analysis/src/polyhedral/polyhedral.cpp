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

#include "insieme/analysis/polyhedral/polyhedral.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"

#include "insieme/utils/iterator_utils.h"

#include <iomanip>

namespace insieme {
namespace analysis {
namespace poly {
//====== Exceptions ===============================================================================
NotAffineExpr::NotAffineExpr(const core::ExpressionPtr& expr): 
		std::logic_error("Expression is not linear and affine"), expr(expr) { }

VariableNotFound::VariableNotFound(const core::VariablePtr& var) : 
		std::logic_error("Variable not found in the iteration vector."), var(var) { }

//====== Element ==================================================================================

bool Element::operator==(const Element& other) const {
	if (this == &other) { return true; }

	if (type == other.type) {
		if(type == ITER || type == PARAM) 
			return *static_cast<const Variable&>(*this).getVariable() == 
				   *static_cast<const Variable&>(other).getVariable();
		else 
			return true;
	}
	return false;
}

bool Element::operator<(const Element& other) const {
    if (type != other.type) { return type < other.type; }
	if (type == ITER || type == PARAM) { 
		return static_cast<const Variable&>(*this).getVariable()->getId() < 
			static_cast<const Variable&>(other).getVariable()->getId(); }
    return false;
}

std::ostream& Iterator::printTo(std::ostream& out) const { return out << *getVariable(); }

std::ostream& Parameter::printTo(std::ostream& out) const { return out << *getVariable(); }

//====== IterationVector ==========================================================================

int IterationVector::getIdx(const Element& elem) const {
	if (const Iterator* iter = dynamic_cast<const Iterator*>(&elem)) {
		return getIdxFrom(*iter, iters);
	}
	if (const Parameter* param = dynamic_cast<const Parameter*>(&elem)) {
		int idx = getIdxFrom(*param, params);
		return (idx==-1?-1:idx+iters.size());	
	}
	assert( dynamic_cast<const Constant*>(&elem) != NULL && "Element not valid." ); 
	return size()-1;
}

const Element& IterationVector::operator[](size_t idx) const { 
	assert(idx >= 0 && idx < size() && "Index out of range");
	if (idx<getIteratorNum()) {
		return iters[idx];
	} 
	if (idx<size()-1) {
		return params[idx-iters.size()];
	}
	return constant;
}

bool IterationVector::operator==(const IterationVector& other) const {
	if (this == &other) {
		return true;
	}
	// check weather the two iterators contain the same elements in the same order
	return std::equal(begin(), end(), other.begin());
}

// An iteration vector is represented by three main components, the iterators, the parameters and
// the constant part. The vector is printed displaying the comma separated list of iterators and
// parameters divided by the '|' separator. 
std::ostream& IterationVector::printTo(std::ostream& out) const {
	out << "(" << join(",", iter_begin(), iter_end(), 
			[&](std::ostream& jout, const Element& cur){ jout << cur; } 
		);
	out << "|";
	out << join(",", param_begin(), param_end(), 
			[&](std::ostream& jout, const Element& cur){ jout << cur; } 
		);
	out << "|1)";
	return out;
}

template <class T>
void merge_add(IterationVector& dest, 
		typename std::vector<T>::const_iterator aBegin, 
		typename std::vector<T>::const_iterator aEnd, 
		typename std::vector<T>::const_iterator bBegin, 
		typename std::vector<T>::const_iterator bEnd ) 
{
	std::set<T> varSet;
    std::set_union(aBegin, aEnd, bBegin, bEnd, std::inserter(varSet, varSet.begin()));
	std::for_each(varSet.begin(), varSet.end(), [&dest](const T& cur) { 
		if ( dest.getIdx(static_cast<const poly::Variable&>(cur).getVariable()) == -1 ) { 
			dest.add(cur); 
		}
	} );
}

// Merges two iteration vectors (a and b) to create a new iteration vector which contains
// both the elements of a and b. 
IterationVector merge(const IterationVector& a, const IterationVector& b) {
	IterationVector ret;

	// because the two iteration vectors are built bottom-up, the iterators in a will not be b and
	// viceversa having the same iterators would mean the same variable has been used as loop
	// iterator index in 1  statement as a parameter in another, therefore we can safely remove the
	// iterators and merge the set of parameters. 
	merge_add<Iterator>(ret, a.iter_begin(), a.iter_end(), b.iter_begin(), b.iter_end());	
	merge_add<Parameter>(ret, a.param_begin(), a.param_end(), b.param_begin(), b.param_end());	
	return ret;
}

const IndexTransMap transform(const IterationVector& trg, const IterationVector& src) {
	assert(trg.size() >= src.size()); //TODO: convert into an exception

	IndexTransMap transMap;
	std::for_each(src.begin(), src.end(), [&](const Element& cur) {
			int idx = 0;
			if (cur.getType() != Element::CONST) {
				idx = trg.getIdx( static_cast<const Variable&>(cur).getVariable() ); 
			} else {
				idx = trg.getIdx(cur);
			}
			assert( idx != -1 && static_cast<size_t>(idx) < trg.size() );
			transMap.push_back( idx ); 
		});
	assert(transMap.size() == src.size());
	return transMap;
}

//====== IterationVector::iterator ================================================================

void IterationVector::iterator::inc(size_t n) {
	if (!valid || n==0) return;

	IterVec::const_iterator&& iterEnd = iterVec.iters.end();
	size_t dist = std::distance(iterIt, iterEnd);
	if (iterIt != iterEnd && dist>=n) {
		iterIt+=n;
		return;
	}
	iterIt = iterEnd;
	n-=dist;

	ParamVec::const_iterator&& paramEnd = iterVec.params.end();
	dist = std::distance(paramIt, paramEnd);
	if (paramIt != paramEnd && dist>=n) {
		paramIt+=n;
		return;
	}
	paramIt = paramEnd;
	n-=dist;

	if (constant) { 
		constant = false;
		valid = false;
	}
}

const Element& IterationVector::iterator::operator*() const {  
	if (!valid) 
		throw IteratorNotValid();

	if (iterIt != iterVec.iters.end())
		return *iterIt;
	else if (paramIt != iterVec.params.end())
		return *paramIt;
	assert(constant && "Iteration vector has no constant part");
	return iterVec.constant;
}

//====== AffineFunction ===========================================================================

AffineFunction::AffineFunction(IterationVector& iterVec, const insieme::core::ExpressionPtr& expr) : 
	iterVec(iterVec), sep(iterVec.getIteratorNum())
{
	using namespace insieme::core::arithmetic;
	// extract the Formula object 
	Formula&& formula = toFormula(expr);
	
	if ( !(formula.isLinear() || formula.isOne()) ) 
		throw NotAffineExpr(expr);

	if ( formula.isOne() ) {
		coeffs.resize(iterVec.size()); // by default the values are initialized to 0
		coeffs.back() = 1;	
		return;
	}

	// this is a linear function
	assert( formula.isLinear() && "Expression is not an affine linear function.");
	
	const std::vector<Formula::Term>& terms = formula.getTerms();
	// we have to updated the iteration vector by adding eventual parameters which are being used by
	// this function. Because by looking to an expression we cannot determine if a variable is an
	// iterator or a parameter we assume that variables in this expression which do not appear in
	// the iteration domain are parameters.
	for_each( terms.begin(), terms.end(), [&](const Formula::Term& cur){ 
		const Product& prod = cur.first;
		assert(prod.getFactors().size() <= 1 && "Not a linear expression");

		if ( !prod.isOne() ) {
			const core::VariablePtr& var = prod.getFactors().front().first;
			// We make sure the variable is not already among the iterators
			if ( iterVec.getIdx( Iterator(var) ) == -1 ) {
				iterVec.add( Parameter(var) );
			}
		}
	});

	// now the iteration vector is inlined with the Formula object extracted from the expression,
	// the size of the coefficient vector can be set.
	coeffs.resize(iterVec.size());
	for_each( terms.begin(), terms.end(), [&](const Formula::Term& cur){ 
		const Product& prod = cur.first;
		assert(prod.getFactors().size() <= 1 && "Not a linear expression");

		if ( prod.isOne() ) {
			coeffs.back() = cur.second;
		} else {
			int idx = iterVec.getIdx( prod.getFactors().front().first );
			assert (idx != -1);
			coeffs[idx] = cur.second;
		}
	});
}

int AffineFunction::idxConv(size_t idx) const {

	if (idx<sep)	{ return idx; }
	if (idx == iterVec.size()-1) { return coeffs.size()-1; }

	if (idx>=sep && idx<iterVec.getIteratorNum()) { return -1; }

	idx -= iterVec.getIteratorNum();
	if (idx<coeffs.size()-sep-1) {
		return sep+idx;
	}
	
	return -1;
}

namespace {
/**
 * Printer: prints affine functions using different styles which can be selected by policies which can are 
 * specified by the user.
 */
struct Printer : public utils::Printable {

	Printer(const AffineFunction& af, unsigned policy) : af(af), policy(policy) { }

	bool doPrintZeros() const { return policy & AffineFunction::PRINT_ZEROS; }

	bool doPrintVars() const { return policy & AffineFunction::PRINT_VARS; }

	template <class IterT>
	void print(std::ostream& out, const IterT& begin, const IterT& end) const {
		bool isEmpty = true;
		out << join((doPrintVars() ? " + " : " "), begin, end, 
				[&](std::ostream& jout, const AffineFunction::Term& cur) { 
					if ( doPrintVars() ) jout << cur;
					else jout << cur.second; 
					isEmpty = false;
				} 
			);
		if(isEmpty) {
			// If the we didn't produce any output it means the affine constraint is all zeros,
			// print the constant part to visualize the real value
			assert(af.getCoeff(Constant()) == 0);
			out << 0;
		}
	}

	std::ostream& printTo(std::ostream& out) const {

		if (!doPrintZeros()) { 
			auto&& filtered = filterIterator<
					AffineFunction::iterator, 
					AffineFunction::Term, 
					AffineFunction::Term*, 
					AffineFunction::Term
				>(af.begin(), af.end(), [](const AffineFunction::Term& cur) -> bool { return cur.second == 0; }
			);
			print(out, filtered.first, filtered.second);
		} else {
			print(out, af.begin(), af.end());
		}

		return out;

	}
		
private:
	const AffineFunction& af;
	const unsigned policy;
};

} // end anonymous namespace


std::string AffineFunction::toStr(unsigned policy) const {
	std::ostringstream ss;
	ss << Printer(*this, policy);
	return ss.str();
}

std::ostream& AffineFunction::printTo(std::ostream& out) const { 
	return out << toStr( AffineFunction::PRINT_VARS );
}

int AffineFunction::getCoeff(size_t idx) const {
	int index = idxConv(idx);
	return (index==-1)?0:coeffs[index];
}

void AffineFunction::setCoeff(const core::VariablePtr& var, int coeff) { 
	int idx = iterVec.getIdx(var);
	// In the case the variable is not in the iteration vector, throw an exception
	if (idx == -1) { throw VariableNotFound(var); }
	setCoeff( idx, coeff );
}

int AffineFunction::getCoeff(const Element& elem) const {
	int idx = iterVec.getIdx(elem);
	// In the case the variable is not in the iteration vector, throw an exception
	assert (idx != -1 && "Element not in iteration vector");
	return getCoeff( idx );
}

int AffineFunction::getCoeff(const core::VariablePtr& var) const { 
	int idx = iterVec.getIdx(var);
	// In the case the variable is not in the iteration vector, throw an exception
	if (idx == -1) { throw VariableNotFound(var); }
	return getCoeff( idx );
}

bool AffineFunction::operator==(const AffineFunction& other) const {
	// in the case the iteration vector is the same, then we look at the coefficients and the
	// separator value to determine if the two functions are the same.
	if(iterVec == other.iterVec)
		return sep == other.sep && std::equal(coeffs.begin(), coeffs.end(), other.coeffs.begin());

	// if the two iteration vector are not the same we need to determine if at least the position
	// for which a coefficient is specified is the same 	
	iterator thisIt = begin(), otherIt = other.begin(); 
	while( thisIt!=end() ) {
		Term &&thisTerm = *thisIt, &&otherTerm = *otherIt;
		const Element::Type& thisType = thisTerm.first.getType();
		const Element::Type& otherType = otherTerm.first.getType();

		if ( (thisType == Element::PARAM && otherType == Element::ITER) || 
			(thisType == Element::CONST && otherType != Element::CONST) ) 
		{
			if (otherTerm.second != 0) { return false; }
			++otherIt;
		} else if ( (thisType == Element::ITER && otherType == Element::PARAM) || 
			(thisType != Element::CONST && otherType == Element::CONST) )	
		{
			if (thisTerm.second != 0) { return false; }
			++thisIt;
		} else if (thisTerm == otherTerm) {
			// iterators aligned 
			++thisIt;
			++otherIt;
		} else return false;
	}	
	return true;
}

AffineFunction AffineFunction::toBase(const IterationVector& iterVec, const IndexTransMap& idxMap) const {
	assert(iterVec.size() >= this->iterVec.size());
	
	IndexTransMap idxMapCpy = idxMap;
	if ( idxMap.empty() ) {
		idxMapCpy = transform(iterVec, this->iterVec);
	}
	AffineFunction ret(iterVec);
	std::fill(ret.coeffs.begin(), ret.coeffs.end(), 0);

	for(size_t it=0; it<this->iterVec.size(); ++it) { 
		ret.coeffs[idxMapCpy[it]] = getCoeff(it);
	}

	return ret;
}

//====== AffineFunction::iterator =================================================================

AffineFunction::iterator& AffineFunction::iterator::operator++() { 
	assert ( iterPos < iterVec.size() && "Iterator not valid!"); 
	
	iterPos++;
	return *this;
}

AffineFunction::Term AffineFunction::iterator::operator*() const {  
	assert ( iterPos < iterVec.size() && "Iterator not valid!"); 

	return Term(iterVec[iterPos], af.getCoeff(iterPos));	
}

//===== Constraint ================================================================================
std::ostream& Constraint::printTo(std::ostream& out) const { 
	out << af << " ";
	switch(type) {
	case EQ: out << "==";  break;	case NE: out << "!=";  break; 	case GT: out << ">";   break;
	case LT: out << "<";   break;	case GE: out << ">=";  break;	case LE: out << "<=";  break;
	}
	return out << " 0";
}

bool Constraint::operator<(const Constraint& other) const {
	if (af.size() == other.af.size()) {	return type < other.type; }
	return af.size() < other.af.size(); 
}

Constraint Constraint::toBase(const IterationVector& iterVec, const IndexTransMap& idxMap) const {
	return Constraint( af.toBase(iterVec, idxMap), type );
}

ConstraintCombinerPtr normalize(const Constraint& c) {
	const Constraint::Type& type = c.getType();
	if ( type == Constraint::EQ || type == Constraint::GE ) { return makeCombiner(c); }

	if ( type == Constraint::NE ) {
		// if the contraint is !=, then we convert it into a negation
		return ~Constraint(c.getAffineFunction(), Constraint::EQ);
	}

	AffineFunction newAf( c.getAffineFunction() );
	// we have to invert the sign of the coefficients 
	if(type == Constraint::LT || type == Constraint::LE) {
		for(AffineFunction::iterator it=c.getAffineFunction().begin(), 
								     end=c.getAffineFunction().end(); it!=end; ++it) 
		{
			newAf.setCoeff((*it).first, -(*it).second);
		}
	}
	if (type == Constraint::LT || type == Constraint::GT) {
		// we have to subtract -1 to the constant part
		newAf.setCoeff(Constant(), newAf.getCoeff(Constant())-1);
	}
	return makeCombiner( Constraint(newAf, Constraint::GE) );
}

//===== ConstraintCombiner ========================================================================

void RawConstraintCombiner::accept(ConstraintVisitor& v) const { v.visit(*this); }
void NegatedConstraintCombiner::accept(ConstraintVisitor& v) const { v.visit(*this); }
void BinaryConstraintCombiner::accept(ConstraintVisitor& v) const { v.visit(*this); }

namespace {

//===== ConstraintPrinter =========================================================================
// Visits the constraints and prints the expression to a provided output stream
struct ConstraintPrinter : public ConstraintVisitor {
	
	std::ostream& out;

	ConstraintPrinter(std::ostream& out) : out(out) { }

	void visit(const RawConstraintCombiner& rcc) { out << "(" << rcc.getConstraint() << ")"; }

	virtual void visit(const NegatedConstraintCombiner& ucc) {
		out << "NOT"; ConstraintVisitor::visit(ucc);
	}

	virtual void visit(const BinaryConstraintCombiner& bcc) {
		out << "(";
		bcc.getLHS()->accept(*this);
		out << (bcc.isConjunction() ? " AND " : " OR ");
		bcc.getRHS()->accept(*this);
		out << ")";
	}

};

} // end anonymous namespace

std::ostream& ConstraintCombiner::printTo(std::ostream& out) const {
	ConstraintPrinter vis(out);
	accept( vis );
	return out;
} 

ConstraintCombinerPtr makeCombiner(const Constraint& constr) {
	return std::make_shared<RawConstraintCombiner>(constr);
}

ConstraintCombinerPtr makeCombiner(const ConstraintCombinerPtr& cc) { return cc; }

namespace {

//===== ConstraintCloner ==========================================================================
// because Constraints are represented on the basis of an iteration vector which is shared among the
// constraints componing a constraint combiner, when a combiner is stored, the iteration vector has
// to be changed. 
struct ConstraintCloner : public ConstraintVisitor {
	ConstraintCombinerPtr newCC;
	const IterationVector& trg;
	const IterationVector* src;
	IndexTransMap transMap;

	ConstraintCloner(const IterationVector& trg) : trg(trg), src(NULL) { }

	void visit(const RawConstraintCombiner& rcc) { 
		const Constraint& c = rcc.getConstraint();
		if (transMap.empty() ) {
			src = &c.getAffineFunction().getIterationVector();
			transMap = transform( trg, *src );
		}
		assert(c.getAffineFunction().getIterationVector() == *src);
		newCC = std::make_shared<RawConstraintCombiner>( c.toBase(trg, transMap) ); 
	}

	virtual void visit(const NegatedConstraintCombiner& ucc) {
		ConstraintVisitor::visit(ucc);
		newCC = std::make_shared<NegatedConstraintCombiner>(newCC);
	}

	virtual void visit(const BinaryConstraintCombiner& bcc) {
		bcc.getLHS()->accept(*this);
		ConstraintCombinerPtr lhs = newCC;

		bcc.getRHS()->accept(*this);
		ConstraintCombinerPtr rhs = newCC;

		newCC = std::make_shared<BinaryConstraintCombiner>( bcc.getType(), lhs, rhs );
	}
};

} // end anonymous namespace 

ConstraintCombinerPtr cloneConstraint(const IterationVector& trgVec, const ConstraintCombinerPtr& old) {
	if (!old) { return ConstraintCombinerPtr(); }
	
	ConstraintCloner cc(trgVec);
	old->accept(cc);
	return cc.newCC;
}

//==== ScatteringFunction ==============================================================================

std::ostream& ScatteringFunction::printTo(std::ostream& out) const {
	out << "IV: " << iterVec << std::endl << "{" << std::endl;
	std::for_each(funcs.begin(), funcs.end(), [&](const AffineFunction& cur) { 
			out << "\t" << cur.toStr(AffineFunction::PRINT_ZEROS) <<  std::endl; 
		} ); 
	return out << "}" << std::endl;
}

void ScatteringFunction::cloneRows(const std::list<AffineFunction>& src) { 
	std::for_each(src.begin(), src.end(), [&] (const AffineFunction& cur) { funcs.push_back(cur.toBase(iterVec)); } );
}

ScatteringFunction& ScatteringFunction::operator=(const ScatteringFunction& other) {
	iterVec = other.iterVec;
	funcs.clear();
	cloneRows(other.funcs);
	return *this;
}


} // end poly namesapce 
} // end analysis namespace 
} // end insieme namespace 

namespace std {

std::ostream& operator<<(std::ostream& out, const insieme::analysis::poly::AffineFunction::Term& c) {
	return out << c.second << "*" << c.first;
}

}

