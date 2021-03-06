#define DEBUG_TYPE "int-rewrite"
#include <llvm/DataLayout.h>
#include <llvm/IRBuilder.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/LLVMContext.h>
#include <llvm/Metadata.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/GetElementPtrTypeIterator.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
//add by wh
#include <llvm/Support/Debug.h>
#include <sstream>

using namespace llvm;

static cl::opt<bool>
WrapOpt("fwrapv", cl::desc("Use two's complement for signed integers"));

namespace {

struct IntRewrite : FunctionPass {
	static char ID;
	IntRewrite() : FunctionPass(ID) {
		PassRegistry &Registry = *PassRegistry::getPassRegistry();
		initializeDominatorTreePass(Registry);
		initializeLoopInfoPass(Registry);
	}

	virtual void getAnalysisUsage(AnalysisUsage &AU) const {
		AU.addRequired<DominatorTree>();
		AU.addRequired<LoopInfo>();
		AU.setPreservesCFG();
	}

	virtual bool runOnFunction(Function &);

private:
	typedef IRBuilder<> BuilderTy;
	BuilderTy *Builder;

	DominatorTree *DT;
	LoopInfo *LI;
	DataLayout *TD;

	bool insertOverflowCheck(Instruction *, Intrinsic::ID, Intrinsic::ID);
	bool insertDivCheck(Instruction *);
	bool insertShiftCheck(Instruction *);
	bool insertArrayCheck(Instruction *);

	bool isObservable(Value *);
};

} // anonymous namespace

static MDNode * findSink(Value *V) {
	for (Value::use_iterator i = V->use_begin(), e = V->use_end(); i != e; ++i) {
		if (Instruction *I = dyn_cast<Instruction>(*i)) {
			if (MDNode *MD = I->getMetadata("sink"))
				return MD;
			if (isa<BinaryOperator>(I) || isa<CastInst>(I))
				return findSink(I);
		}
	}
	return NULL;
}

static Instruction * insertIntSat(Value *V, Instruction *I, Instruction *IP, 
		StringRef Bug, const DebugLoc &DbgLoc) {
	Module *M = IP->getParent()->getParent()->getParent();
	LLVMContext &C = M->getContext();
	FunctionType *T = FunctionType::get(Type::getVoidTy(C), Type::getInt1Ty(C), false);
	Function *F = cast<Function>(M->getOrInsertFunction("int.sat", T));
	F->setDoesNotThrow();
	CallInst *CI = CallInst::Create(F, V, "", IP);
	CI->setDebugLoc(DbgLoc);
	// Embed operation name in metadata.
	MDNode *MD = MDNode::get(C, MDString::get(C, Bug));
	CI->setMetadata("bug", MD);

	// Add taint metadata
	for (unsigned i = 0; i < I->getNumOperands(); ++i) {
		if (MDNode *MD = I->getMetadata("taint")) {
			CI->setMetadata("taint", MD);
			break;
		}
	}
	// Add sink metadata
	if (MDNode *MD = findSink(I))
		CI->setMetadata("sink", MD);

	return CI;
}

///////////////////wh
void toString(Instruction *I, std::string &Str) {
	llvm::raw_string_ostream OS(Str);
	I->getOperand(0)->getType()->print(OS);;
}

void toString2(Value *V, std::string &Str) {
	llvm::raw_string_ostream OS(Str);
	V->getType()->print(OS);
}
	
void ExtendAnno(StringRef &Anno, Instruction *I) {
	std::string m_Anno = Anno.str();
	std::string type;
	switch(I->getOpcode()) {
		default: break;
		case Instruction::Add:
		case Instruction::Sub:
		case Instruction::Mul:			
		case Instruction::SDiv:
		case Instruction::UDiv:	
			m_Anno.append(".");
			toString(I, type);
			m_Anno.append(type);
			Anno=StringRef(m_Anno);
			break;
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			m_Anno.append(".");
			toString(I, type);
			m_Anno.append(type);
			Anno=StringRef(m_Anno);
			break;
		case Instruction::GetElementPtr:
			gep_type_iterator i = gep_type_begin(I), e = gep_type_end(I);
			for (; i != e; ++i) {//I->getPrevNode()->getPrevNode()->dump();
				ArrayType *T = dyn_cast<ArrayType>(*i);
				if (!T)
					continue;
				uint64_t n = T->getNumElements();
				if (n <= 1)
					n = INT_MAX;
				m_Anno.append(".size");
				Value *Idx = i.getOperand();
				toString2(Idx, type);
				std::ostringstream size;
				size<<n;
				m_Anno.append(size.str());
				m_Anno.append(".");
				m_Anno.append(type);
			}			
			Anno=StringRef(m_Anno);
			break;
	}
}

/*
 * check if the operators of binary operation own compare instruction before the operation
 */
bool check_pre_cmp_condition(Instruction *I) {  
	if(isa<BinaryOperator>(I)){ 
		llvm::BasicBlock *BB = I->getParent();
		//llvm::dbgs()<<"BB = "<<BB->getName()<<'\n';
		if (BB) {
			pred_iterator PI = pred_begin(BB), E = pred_end(BB);
			if (PI != E) {//BB has predecessors
				llvm::BasicBlock *Pred;
				for (;PI != E; ++PI) {
					Pred = *PI;
					if(Pred){
					// llvm::dbgs()<<Pred->getName()<<" ";
						for(llvm::ilist_iterator<llvm::Instruction> j = Pred->begin(), f = Pred->end();j != f; ++j) {
							Instruction *J = &*j;
							if(isa<ICmpInst>(J)){//check satisfiable by comparing operands
								if(I->getOperand(0)==J->getOperand(0) ||
								  I->getOperand(0)==J->getOperand(1) ||
								  I->getOperand(1)==J->getOperand(0) ||
								  I->getOperand(1)==J->getOperand(1) ){
									//llvm::dbgs()<<"operand 0 is same.\n";
									//find satisfiable icmp instruction in this Pred
									//don't need to check other instructions
									return true;
									break;
								}
							}
						}
					}
				}
			}  
			//no satisfiable icmp instruction in all predecessors
			//or no predecessors			
			llvm::ilist_iterator<llvm::Instruction> m = BB->begin();
			Instruction *M = &*m;
			for(;M != I; ++m, M = &*m) {
				if(isa<ICmpInst>(M)){
					if(I->getOperand(0)==M->getOperand(0) ||
					  I->getOperand(0)==M->getOperand(1) ||
					  I->getOperand(1)==M->getOperand(0) ||
					  I->getOperand(1)==M->getOperand(1) ) {//find satisfiable icmp instruction in previous instructions
						return true;
						break;
					}
				}
			}				
		}		  	  
	}
	else if(isa<GetElementPtrInst>(I)){
		return true;
	}
	return false;	
}

/*
 * check if the result of binary operation is related to the following call instruction of function about memory
 * such as *alloc*(kmalloc, vmalloc, kzalloc etc.), mem*(memcpy, memset etc.), copy_from_user
 */
bool check_fol_call_condition(Instruction *I) {
	llvm::BasicBlock *BB = I->getParent();	
	if (!BB)
		return false;
	llvm::Function *F = BB->getParent();
	if (!F)
		return false;
	bool find_I = false;
	//check all instructions under I of the function of the instruction I itself
	for (llvm::ilist_iterator<llvm::Instruction> i = BB->begin(); i != BB->end(); i++) {
	//for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *it = &*i;
		if (it == I)//find I
			find_I = true;
		if (find_I) {//the function call must be under the instruction I
			CallInst *CI = dyn_cast<CallInst>(&*i);
			if (CI && CI->getCalledFunction()) {
				StringRef func_name = CI->getCalledFunction()->getName();
				if (func_name.find("alloc") != StringRef::npos || 
				    func_name.find("memcpy") != StringRef::npos || 
				    func_name.find("memmove") != StringRef::npos || 
				    func_name.find("memset") != StringRef::npos || 
				    func_name.find("copy_from_user") != StringRef::npos ||
				    func_name.find("copy_to_user") != StringRef::npos ||
				    func_name.find("copy_in_user") != StringRef::npos) {
					int arg_num = CI->getNumArgOperands();
					for (int ai = 0; ai < arg_num; ai++) {
						Value *arg = CI->getArgOperand(ai);
						if (arg == I) {
					//llvm::dbgs()<<F->getName()<<"::"<<func_name<<'\n';
					//I->dump();
							return true;
						}
					}
				}
			}
		}
	}
	//check the name of the function of I
	//if it contain some key words (such as "ioctl", "memory", etc.), the function maybe attacked by integer overflow
	StringRef func_name_of_I = F->getName();
	if (func_name_of_I.find("ioctl") != StringRef::npos ||
	    func_name_of_I.find("memory") != StringRef::npos) {
		return true;
	}
	return false;	
}
/*
 * check if the 'user' is one user of 'v''s users 
 */
bool check_use_relation(Value* v, Instruction* user) 
{//llvm::dbgs()<<"find data input functions\n";
	if(v->use_empty() || !v || !user) {//if v's users are empty or v or user are null
		return false;
	}
	//check each user of v
	for (Value::use_iterator i = v->use_begin(), e = v->use_end(); i != e; ++i) {
		Value *ui = *i;
		Instruction *uii = cast<Instruction>(ui);
		if(uii && uii == user)//if a user of v is Instruction and is equal to 'user'
			return true;//it represent the 'user' is related to 'v'
		else {//if not, check the users of ui recursively
			if(check_use_relation(ui, user))
				return true;
		}
	}
	return false;
}
/*
 * check if the operators are related with functions like read(), fread() and recv()
 */
bool check_data_input_condition(Instruction *I)
{
	if(!I)
		return false;
	llvm::BasicBlock *BB = I->getParent();	
	if (!BB)
		return false;
	//check binary operator
	if(isa<BinaryOperator>(I)) {
		for(llvm::ilist_iterator<llvm::Instruction> i = BB->begin(); i != BB->end(); i++) {
			Instruction *it = &*i;
			if(it == I)//only check instructions before I
				break;
			CallInst *CI = dyn_cast<CallInst>(&*i);
			if (CI && CI->getCalledFunction()) {//if the instruction is CallInst and the function it called is existed
				StringRef func_name = CI->getCalledFunction()->getName();
				//check if the function called is fread, read or recv
				if(func_name.find("fread") != StringRef::npos) {//llvm::dbgs()<<"find data input functions\n";
					//check if the instrucion I are related to the first arguement of function fread
					//size_t fread ( void *buffer, size_t size, size_t count, FILE *stream) ;
					if(check_use_relation(CI->getArgOperand(0), I))
						return true;
				} else if(func_name.find("read") != StringRef::npos) {//llvm::dbgs()<<"find data input functions\n";
					//check if the instrucion I are related to the second arguement of function read
					//ssize_t read(int fd,void *buf,size_t nbyte) 
					if(check_use_relation(CI->getArgOperand(1), I))
						return true;
				} else if(func_name.find("recv") != StringRef::npos) {//llvm::dbgs()<<"find data input functions\n";
					//check if the instrucion I are related to the second arguement of function recv
					//int recv(int s, char FAR *buf, int len, int flags);
					//int recvfrom(int s, char FAR* buf, int len, int flags, struct sockaddr FAR *from, int FAR *fromlen);
					if(check_use_relation(CI->getArgOperand(1), I))
						return true;
				}
			}
		}
		return false;
	}
	//check getelementptrinst(array index)
	else if(isa<GetElementPtrInst>(I)) {
		return true;
	}
	return false;
	
}
///////////////////////////////

void insertIntSat(Value *V, Instruction *I, StringRef Bug) {
	insertIntSat(V, I, I, Bug, I->getDebugLoc());
}

void insertIntSat(Value *V, Instruction *I) {
	///////add by wh
	StringRef Anno(I->getOpcodeName());
	ExtendAnno(Anno, I);
	insertIntSat(V, I, Anno);
	////////////////
	//insertIntSat(V, I, I->getOpcodeName());
}

bool IntRewrite::runOnFunction(Function &F) {
	BuilderTy TheBuilder(F.getContext());
	Builder = &TheBuilder;
	DT = &getAnalysis<DominatorTree>();
	LI = &getAnalysis<LoopInfo>();
	TD = getAnalysisIfAvailable<DataLayout>();
	bool Changed = false;
	//////////////add by wh
	bool findSatICmp = false;
	
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		if (!isa<BinaryOperator>(I) && !isa<GetElementPtrInst>(I))
			continue;
		Builder->SetInsertPoint(I);
		
		//////////////////////////////////////////////wh
		//findSatICmp = (check_pre_cmp_condition(I) && check_fol_call_condition(I));
		//findSatICmp = (check_pre_cmp_condition(I) && check_fol_call_condition(I)) || check_data_input_condition(I);
		//findSatICmp = true;
		
	//	if(findSatICmp){
			switch (I->getOpcode()) {
			default:
				findSatICmp = false;
				continue;
			case Instruction::Add:
				Changed |= insertOverflowCheck(I,
					  Intrinsic::sadd_with_overflow,
					  Intrinsic::uadd_with_overflow);
				break;
			case Instruction::Sub:
				Changed |= insertOverflowCheck(I,
					  Intrinsic::ssub_with_overflow,
					  Intrinsic::usub_with_overflow);
				break;
			case Instruction::Mul:
				Changed |= insertOverflowCheck(I,
					  Intrinsic::smul_with_overflow,
					  Intrinsic::umul_with_overflow);
				break;
			case Instruction::SDiv:
			case Instruction::UDiv:
				Changed |= insertDivCheck(I);
				break;
			// Div by zero is not rewitten here, but done in a separate
			// pass before -overflow-idiom, which runs before this pass
			// and rewrites divisions as multiplications.
			case Instruction::Shl:
			case Instruction::LShr:
			case Instruction::AShr:
				Changed |= insertShiftCheck(I);
				break;
			case Instruction::GetElementPtr:
				Changed |= insertArrayCheck(I);
				break;
			}
			//reset findSatICmp flag
	//		findSatICmp = false;
	//	}
		////////////////////////////////////////////////
		
/*		switch (I->getOpcode()) {
		default: continue;
		case Instruction::Add:
			Changed |= insertOverflowCheck(I,
				Intrinsic::sadd_with_overflow,
				Intrinsic::uadd_with_overflow);
			break;
		case Instruction::Sub:
			Changed |= insertOverflowCheck(I,
				Intrinsic::ssub_with_overflow,
				Intrinsic::usub_with_overflow);
			break;
		case Instruction::Mul:
			Changed |= insertOverflowCheck(I,
				Intrinsic::smul_with_overflow,
				Intrinsic::umul_with_overflow);
			break;
		case Instruction::SDiv:
		case Instruction::UDiv:
			Changed |= insertDivCheck(I);
			break;
		// Div by zero is not rewitten here, but done in a separate
		// pass before -overflow-idiom, which runs before this pass
		// and rewrites divisions as multiplications.
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
			Changed |= insertShiftCheck(I);
			break;
		case Instruction::GetElementPtr:
			Changed |= insertArrayCheck(I);
			break;
		}
*/	}
	return Changed;
}

bool IntRewrite::isObservable(Value *V) {
	Instruction *I = dyn_cast<Instruction>(V);
	if (!I)
		return false;
	BasicBlock *BB = I->getParent();
	switch (I->getOpcode()) {
	default: break;
	case Instruction::Br:
	case Instruction::IndirectBr:
	case Instruction::Switch:
		if (Loop *L = LI->getLoopFor(BB)) {
			if (L->isLoopExiting(BB))
				return true;
		}
		return false;
	}
	// Default: observable if unsafe to speculately execute.
	return !isSafeToSpeculativelyExecute(I, TD);
}

bool IntRewrite::insertOverflowCheck(Instruction *I, Intrinsic::ID SID, Intrinsic::ID UID) {
	// Skip pointer subtraction, where LLVM converts both operands into
	// integers first.
	Value *L = I->getOperand(0), *R = I->getOperand(1);
	if (isa<PtrToIntInst>(L) || isa<PtrToIntInst>(R))
		return false;

	bool hasNSW = cast<BinaryOperator>(I)->hasNoSignedWrap();
	Intrinsic::ID ID = hasNSW ? SID : UID;
	Module *M = I->getParent()->getParent()->getParent();
	Function *F = Intrinsic::getDeclaration(M, ID, I->getType());
	CallInst *CI = Builder->CreateCall2(F, L, R);
	Value *V = Builder->CreateExtractValue(CI, 1);
	// llvm.[s|u][add|sub|mul].with.overflow.*
	StringRef Anno = F->getName().substr(5, 4);
	
	////////////////wh
	ExtendAnno(Anno, I);
	////////////////
	
	if (hasNSW) {
		// Insert the check eagerly for signed integer overflow,
		// if -fwrapv is not given.
		if (!WrapOpt) {
			insertIntSat(V, I, Anno);
			return true;
		}
		// Clear NSW flag given -fwrapv.
		cast<BinaryOperator>(I)->setHasNoSignedWrap(false);
	}

	// Defer the check.
	SmallPtrSet<Value *, 16> Visited;
	SmallVector<Value *, 16> Worklist;
	typedef SmallPtrSet<BasicBlock *, 16> ObSet;
	ObSet ObPoints;
	BasicBlock *BB = I->getParent();

	Worklist.push_back(I);
	Visited.insert(I);
	while (!Worklist.empty()) {
		Value *E = Worklist.back();
		Worklist.pop_back();
		for (Value::use_iterator i = E->use_begin(), e = E->use_end(); i != e; ++i) {
			User *U = *i;
			// Observable point.
			if (isObservable(U)) {
				// U must be an instruction for now.
				BasicBlock *ObBB = cast<Instruction>(U)->getParent();
				// If the instruction's own BB is an observation
				// point, a check will be performed there, so
				// there is no need for other checks.
				//
				// If ObBB is not dominated by BB (e.g., due to
				// loops), fall back.
				if (ObBB == BB || !DT->dominates(BB, ObBB)) {
					insertIntSat(V, I, Anno);
					return true;
				}
				ObPoints.insert(ObBB);
				continue;
			}
			// Add to worklist if new.
			if (U->use_empty())
				continue;
			if (Visited.insert(U))
				Worklist.push_back(U);
		}
	}

	const DebugLoc &DbgLoc = I->getDebugLoc();
	for (ObSet::iterator i = ObPoints.begin(), e = ObPoints.end(); i != e; ++i) {
		BasicBlock *ObBB = *i;
		insertIntSat(V, I, ObBB->getTerminator(), Anno, DbgLoc);
	}
	return true;
}

bool IntRewrite::insertDivCheck(Instruction *I) {
	Value *R = I->getOperand(1);
	// R == 0.
	Value *V = Builder->CreateIsNull(R);
	// L == INT_MIN && R == -1.
	if (I->getOpcode() == Instruction::SDiv) {
		Value *L = I->getOperand(0);
		IntegerType *T = cast<IntegerType>(I->getType());
		unsigned n = T->getBitWidth();
		Constant *SMin = ConstantInt::get(T, APInt::getSignedMinValue(n));
		Constant *MinusOne = Constant::getAllOnesValue(T);
		V = Builder->CreateOr(V, Builder->CreateAnd(
			Builder->CreateICmpEQ(L, SMin),
			Builder->CreateICmpEQ(R, MinusOne)));
	}
	insertIntSat(V, I);
	return true;
}

bool IntRewrite::insertShiftCheck(Instruction *I) {
	Value *Amount = I->getOperand(1);
	IntegerType *T = cast<IntegerType>(Amount->getType());
	Constant *C = ConstantInt::get(T, T->getBitWidth());
	Value *V = Builder->CreateICmpUGE(Amount, C);
	insertIntSat(V, I);
	return true;
}

bool IntRewrite::insertArrayCheck(Instruction *I) {
	Value *V = NULL;
	gep_type_iterator i = gep_type_begin(I), e = gep_type_end(I);
	for (; i != e; ++i) {
		// For arr[idx], check idx >u n.  Here we don't use idx >= n
		// since it is unclear if the pointer will be dereferenced.
		ArrayType *T = dyn_cast<ArrayType>(*i);
		if (!T)
			continue;
		uint64_t n = T->getNumElements();
		Value *Idx = i.getOperand();
		Type *IdxTy = Idx->getType();
		assert(IdxTy->isIntegerTy());
		// a[0] or a[1] are weird idioms used at the end of a struct.
		// Use the maximum signed value instead for the upper bound.
		if (n <= 1)
			n = INT_MAX;
		Value *Check = Builder->CreateICmpUGT(Idx, ConstantInt::get(IdxTy, n));
		if (V)
			V = Builder->CreateOr(V, Check);
		else
			V = Check;
	}
	if (!V)
		return false;
	
	////////////wh
	StringRef Anno("array");
	ExtendAnno(Anno, I);
	insertIntSat(V, I, Anno);
	////////////	
	
	//insertIntSat(V, I, "array");
	return true;
}

char IntRewrite::ID;

static RegisterPass<IntRewrite>
X("int-rewrite", "Insert int.sat calls", false, false);
