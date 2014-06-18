#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <list>

using namespace std;

long var_count = 0;

bool need_reverse = false;

enum ovf_type {//整数溢出类型的枚举
	UADD,
	SADD,
	USUB,
	SSUB,
	UMUL,
	SMUL,
	UDIV,
	SDIV,
	SHL,
	LSHR,
	ASHR,
	ARRAY,
	SIZE
};

//整数溢出错误信息的结构体
typedef struct ovf_info {
	string type;//错误类型
	int type_no;//错误类型的编号
	int bit_or_size;//整数的位数或者数组索引值的大小
	int pos;//
	string func_name;//size类型错误的函数名称
}oi;

//整数溢出位置信息的结构体
typedef struct loc_info {
	string path;//源码路径信息
	int line;//错误的行数
	oi bug;//错误信息
}li;


//提取整数溢出错误的位置信息
//位置字符串格式：path/file：line：row
void extract_loc_info(string *str, li &loc) {
	int path_start = 0;//源码路径起始位置
	int path_end = 0;//源码路径结束位置
	int line_end = 0;//行数结束位置
	int index = 0, count = 0;//count表示在字符串中找到的冒号的数量
	
	//存在‘/’说明是位置字符串
	if((path_start = str->find_first_of('/')) != string::npos) {
		//设置源码路径结束位置和行数结束位置
		while((index = str->find_first_of(':', index)) != string::npos) {
			//cout<<index<<'\n';
			if(count == 0)
				path_end = index;
			else if(count == 1)
				line_end = index;
			index++;
			count++;
		}    
		//cout<<path_start<<"  "<<path_end<<"  "<<line_end<<'\n';
		
		//获取源码路径信息，包括文件名
		loc.path = str->substr(path_start, path_end - path_start);
		//cout<<loc.path<<'\n';
		
		//获取行数信息
		loc.line = atoi(str->substr(path_end + 1, line_end - path_end).c_str()); 
		//cout<<loc.line<<'\n';
	}
}

//提取错误信息
void extract_ovf_info(string *str, oi &bug) {
	int type_start = 0;//提取内容的起始位置
	int type_end = 0;//提取的第一个信息的结束位置
	int type_end2 = 0;//提取的第二个信息的结束位置
	int type_end3 = 0;//提取的第三个信息的结束位置
	
	//寻找字符串中第一个不为空格的字符的位置
	if((type_start = str->find_first_of(' ')) != string::npos) {
		//找到第一个‘.’的位置
		if((type_end = str->find_first_of('.')) != string::npos) {
			//提取出第一个信息--错误类型信息
			bug.type = str->substr(type_start + 1, type_end - type_start - 1);
			//找到第二个'.'的位置并提取第二个信息--位数或者数组大小
			if((type_end2 = str->find_first_of('.', type_end + 1)) != string::npos) {
				bug.bit_or_size = atoi(str->substr(type_end + 2, type_end2 - type_end - 2).c_str());
			}
			//没有找到则说明则直接提取第二个信息--位数或者数组大小
			else 
				bug.bit_or_size = atoi(str->substr(type_end + 2, str->size() - type_end - 2).c_str());
			//根据错误类型设置错误类型编号，字符串格式：类型.i位数
			if(bug.type == string("uadd"))
				bug.type_no = UADD;
			else if(bug.type == string("sadd"))
				bug.type_no = SADD;
			else if(bug.type == string("usub"))
				bug.type_no = USUB;
			else if(bug.type == string("ssub"))
				bug.type_no = SSUB;
			else if(bug.type == string("umul"))
				bug.type_no = UMUL;
			else if(bug.type == string("smul"))
				bug.type_no = SMUL;
			else if(bug.type == string("udiv"))
				bug.type_no = UDIV;
			else if(bug.type == string("sdiv"))
				bug.type_no = SDIV;
			else if(bug.type == string("shl"))
				bug.type_no = SHL;
			else if(bug.type == string("lshr"))
				bug.type_no = LSHR;
			else if(bug.type == string("ashr"))
				bug.type_no = ASHR;
			//array类型的字符串格式：array.m数组大小.i位数
			else if(bug.type == string("array")) {
				bug.type_no = ARRAY;
				bug.pos = atoi(str->substr(type_end2 + 2, str->size() - type_end2 - 2).c_str());
			}
			//size类型的字符串格式：size.i位数.p位置.函数名称
			else if(bug.type == string("size")) {
				bug.type_no = SIZE;
				//获取可能溢出的参数的位置
				bug.pos = atoi(str->substr(type_end2 + 2, 1).c_str());
				//获取函数的名称
				if((type_end3 = str->find_first_of('.', type_end2 + 1)) != string::npos) {
				      bug.func_name = str->substr(type_end3 + 1, str->size() - type_end3 - 2);
				      //如果函数名中存在"llvm"字符串，则去除掉
				      if(bug.func_name.find("llvm.") != string::npos) {
					      int func_start = 0;//函数名起始位置
					      int func_end = 0;//函数名结束位置
					      func_start = bug.func_name.find_first_of('.');
					      func_end = bug.func_name.find_first_of('.', func_start + 1);
					      if(func_end != string::npos && func_start != string::npos) {
						      bug.func_name = bug.func_name.substr(func_start + 1, func_end - func_start - 1);
					      }
					      else {
						      cout<<"extract llvm mem function failed\n";
						      exit(0);
					      }
				      }
				      //cout<<bug.func_name<<'\n';
				}
			}
			//未识别的错误类型
			else {
				cout<<"find unrecoganized overflow type\n";
				cout<<bug.type<<endl;
				//exit(0);
			}
		}
	}
	
	//cout<<bug.type<<"."<<bug.bit_or_size<<'\n';
}

//copy from Internet
//将字符串转换为整型
int my_itoa(int val, char* buf) {
	const unsigned int radix = 10;
	char *p;
	unsigned int a;
	int len;
	char* b; //start of the digit char
	char temp;
	unsigned int u;
	p = buf;
	if (val < 0) {
		*p++ = '-';
		val = 0 - val;
	}
	u = (unsigned int)val;
	b = p;
	do {
		a = u % radix;
		u /= radix;
		*p++ = a + '0';
	} while (u > 0);
	len = (int)(p - buf);
	*p-- = 0;
	//swap
	do {
		temp = *p;
		*p = *b;
		*b = temp;
		--p;
		++b;
	} while (b < p);
	return len;
}

//判断字符是否是26个字母
bool isCharacter(char ch) {
	if(ch >= 'a' && ch <= 'z')
		return true;
	else if(ch >= 'A' && ch <= 'Z')
		return true;
	else
		return false;
}

//判断字符是否是数字
bool isNumber(char ch) {
	if(ch >= '0' && ch <= '9')
		return true;
	else
		return false;
}

//判断字符是否是运算符
bool isOperation(char ch) {
	if(ch == '+'||ch == '-'||ch == '*'||ch == '/')
		return true;
	else
		return false;
}

//获取运算符的运算优先级
int getOperationLevel(char ch) {
	if(ch == '-' || ch == '+')//+和-的优先级为1
		return 1;
	else if(ch == '*' || ch == '/')//*和/的优先级为2
		return 2;
	else if(ch == '<' || ch == '>')//<和>的优先级为3
		return 3;
	else//其余的无优先级
		return -1;		
}

//提取运算数code代表待提取的字符串
//isleft表示是否是左运算数
//operand用来指向提取后的运算数
//op_pos表示运算符的位置
//start表示离运算符最近的分号的位置
bool extract_operand(string *code, bool isleft, string *operand, int op_pos, int start) {
	//code为空
	if(code == NULL) {
		cout<<"string is null in extract_operand\n";
		return false;
	}
	//没有运算符的位置，无法提取
	if(op_pos <= 0) {
		cout<<"opcode is not find\n";
		return false;
	}
	
	//提取左运算数
	if(isleft) {
		int count = op_pos - 1;	//左运算数从运算符位置的左边开始提取
		int meet_right_bracket = 0;//遇到的右括号的数量
		int meet_right_square = 0;//遇到的右方括号的数量
		
		//cout<<code->at(count)<<'\n';
		//循环地从运算符位置向左提取一个个字符
		//如果遇到=或者{，表示提取结束
		while(count > start && code->at(count) != '=' && code->at(count) != '{') {
			//如果提取到的字符是运算符，则比较其与运算符位置上的运算符的优先级的大小
			//如果优先级小于运算符位置上的运算符的优先级并且遇到的右括号数量为0,那么提取结束
			if(isOperation(code->at(count))) {
				if(getOperationLevel(code->at(count)) < getOperationLevel(code->at(op_pos)) && meet_right_bracket == 0)
					break;
			}			
			
			//当遇到"->"或者">>"时，直接将其作为运算数的一部分
			else if(code->at(count) == '>') {//recoganize "->"
				if(code->at(count - 1) == '-' || code->at(count - 1) == '>') {
					operand->insert(0, 1, code->at(count));
					operand->insert(0, 1, code->at(count - 1));
					count -= 2;
					continue;
				}
				else
					break;
			} 
			
			//当遇到"<<"时，也直接将其作为运算数的一部分
			else if(code->at(count) == '<') {//not <<
				if(code->at(count - 1) == '<') {
					operand->insert(0, 1, code->at(count));
					operand->insert(0, 1, code->at(count - 1));
					count -= 2;
					continue;
				}
				else	
					break;
			}
			
			//当遇到右括号时，将其作为运算数的一部分，并使meet_right_bracket加1
			else if(code->at(count) == ')') {
				operand->insert(0, 1, code->at(count));
				//if(meet_right_bracket == 0) {
					meet_right_bracket ++;
					count--;
					continue;
				//}
				//meet_right_bracket++;
			}
			
			//当遇到左括号时，如果此时遇到的右括号数不为0,表示该左括号也是运算数的一部分
			//否则不是运算数的一部分，应该结束提取过程
			else if(code->at(count) == '(') {//'(' must have a corresponding ')'
				if(meet_right_bracket != 0) {
					meet_right_bracket--;
					//if(meet_right_bracket != 0)
					operand->insert(0, 1, code->at(count));
					count--;
					continue;
				}
				else
					break;
			}
			
			//如果遇到右方括号，将其作为运算数的一部分，并使meet_right_square加1
			else if(code->at(count) == ']') {
				operand->insert(0, 1, code->at(count));
				meet_right_square ++;
				count--;
				continue;
			}
			
			//当遇到左方括号时，如果此时遇到的右方括号数不为0,表示该左方括号也是运算数的一部分
			//否则不是运算数的一部分，应该结束提取过程
			else if(code->at(count) == '[') {//'(' must have a corresponding ')'
				if(meet_right_square != 0) {
					meet_right_square--;
					//if(meet_right_bracket != 0)
					operand->insert(0, 1, code->at(count));
					count--;
					continue;
				}
				else
					break;
			}
			
			//当遇到逗号时，如果遇到的右括号数不为0,则表示该逗号属于运算数的一部分
			else if(code->at(count) == ',') {//',' must be in bracket pair
				if(meet_right_bracket != 0) {
					operand->insert(0, 1, code->at(count));
					count--;
					continue;
				}
				else
					break;
			}
			
			//当遇到空格时
			else if(code->at(count) == ' ') {
				if(count == code->find_first_of(' ', start) && operand->size() != 0 && 
				   operand->find_first_not_of(' ') != string::npos && meet_right_bracket == 0 &&
				   meet_right_square == 0) {
					cout<<"jump space\n";
					break;
				}
			}
			//遇到'\t'时直接跳过该字符
			else if(code->at(count) == '\t') {
				count--;
				continue;
			}
			//遇到'?'时并且meet_right_bracket为0直接结束
			else if(code->at(count) == '?') {//for "? : "
				if(meet_right_bracket == 0) {
					break;
				}
				else {
					return false;
				}
			}
			//遇到一个冒号并且meet_right_bracket为0时结束，遇到两个则继续
			else if(code->at(count) == ':') {
				if(code->at(count - 1) != ':') {
					if(meet_right_bracket == 0) {
						break;
					}
					else {
						return false;
					}
				}
			}
			operand->insert(0, 1, code->at(count));
			count--;
		}
		cout<<*operand<<'\n';
	}
	//提取右运算数
	else {//extract right operand
		int count = op_pos + 1;		
		//bool hasspace = false;
		int meet_left_bracket = 0;
		int meet_left_square = 0;
		//string operand_reverse("");
		
		//if meet +=|-=|*=|/=, ignore '='
		if(code->at(count) == '=') {
			count++;
		}
		
		//cout<<code->at(count)<<'\n';
		while(count < code->size() && code->at(count) != ';' && code->at(count) != '}') {
		  
			if(code->at(count) == '-') {//recoganize "->"
				if(code->at(count + 1) == '>') {
					operand->insert(operand->size(), 1, code->at(count));
					operand->insert(operand->size(), 1, code->at(count + 1));
					count += 2;
					continue;
				}
			} 
			
			if(isOperation(code->at(count))) {
				if(getOperationLevel(code->at(count)) <= getOperationLevel(code->at(op_pos)) && meet_left_bracket == 0)
					break;
			}	
			
			else if(code->at(count) == '>') {
				if(code->at(count + 1) == '>' && (code->at(op_pos) != '>' || code->find(">>=") != string::npos) && (code->at(op_pos) != '<' || code->find("<<=") != string::npos)) {
					operand->insert(operand->size(), 1, code->at(count));
					operand->insert(operand->size(), 1, code->at(count + 1));
					count += 2;
					continue;
				}
				else
					break;
			}
			
			else if(code->at(count) == '<') {
				//for x <<= a<<b;
				if(code->at(count + 1) == '<' && (code->at(op_pos) != '>' || code->find(">>=") != string::npos) && (code->at(op_pos) != '<' || code->find("<<=") != string::npos)) {
					operand->insert(operand->size(), 1, code->at(count));
					operand->insert(operand->size(), 1, code->at(count + 1));
					count += 2;
					continue;
				}
				else
					break;
			}
			
			else if(code->at(count) == '(') {
				//operand->insert(0, 1, code->at(count));
				meet_left_bracket++;
			}
			
			else if(code->at(count) == ')') {//cout<<"meet right bracket\n";//'(' must have a corresponding ')'
				if(meet_left_bracket != 0) {//cout<<"meet_left_bracket "<<meet_left_bracket<<endl;
					meet_left_bracket--;
					operand->insert(operand->size(), 1, code->at(count));
					count++;
					continue;
				}
				else
					break;
			}
			
			else if(code->at(count) == '[') {
				//operand->insert(0, 1, code->at(count));
				meet_left_square++;
			}
			
			else if(code->at(count) == ']') {//'(' must have a corresponding ')'
				if(meet_left_square != 0) {
					meet_left_square--;
					operand->insert(operand->size(), 1, code->at(count));
					count++;
					continue;
				}
				else
					break;
			}
			
			else if(code->at(count) == ',') {//',' must be in bracket pair
				if(meet_left_bracket != 0) {
					operand->insert(operand->size(), 1, code->at(count));
					count++;
					continue;
				}
				else
					break;
			}
			
			else if(code->at(count) == '\t') {
				count++;
				continue;
			}
			
			else if(code->at(count) == ':') {
				if(code->at(count + 1) == ':' || meet_left_bracket != 0) {
					operand->insert(operand->size(), 1, code->at(count));
					operand->insert(operand->size(), 1, code->at(count + 1));
					count += 2;
					continue;
				}
				else
					break;
			}
			else if(code->at(count) == '=') {
				if(meet_left_bracket == 0)
					break;
			}
			else if(code->at(count) == '!') {
				if(meet_left_bracket == 0)
					break;
			}
			else if(code->at(count) == '?') {
				if(meet_left_bracket == 0)
					break;
			}
			operand->insert(operand->size(), 1, code->at(count));
			count++;
		}
		cout<<*operand<<'\n';
	}
	if(operand->size() == 0 || operand->find_first_not_of(' ') == string::npos) {
		return false;
	}
	return true;
}

//提取size类型的整数错误的函数的参数
bool extract_func_arg(string *code, string func_name, int arg_pos, string *loperand) {
	int func_start = 0;
	string args[4]={"", "", "", ""};
	
	if((func_start = code->find(func_name)) == string::npos) {
		cout<<"can't find function\n";
		return false;
	}cout<<"func_start = "<<func_start<<endl;
	
	int left_bracket = func_start, right_bracket = func_start;
	int left_bracket_num = 0, right_bracket_num = 0;
	int comma_pos = func_start, comma_num = 0, pre_comma = code->find_first_of('(', func_start + 1);
	int arg_no = 1;
	
	while((comma_pos = code->find_first_of(',', comma_pos + 1)) != string::npos && arg_no <= 4) {
		cout<<"comma_pos = "<<comma_pos<<endl;
		cout<<"pre_comma = "<<pre_comma<<endl;
	  
		while(code->find_first_of('(', left_bracket + 1) != string::npos && code->find_first_of('(', left_bracket + 1) < comma_pos) {
			left_bracket = code->find_first_of('(', left_bracket + 1);			
			left_bracket_num++;cout<<"left_bracket = "<<left_bracket<<endl;
		}
		
		while(code->find_first_of(')', right_bracket + 1) != string::npos && code->find_first_of(')', right_bracket + 1) < comma_pos) {
			right_bracket = code->find_first_of(')', right_bracket + 1);
			right_bracket_num++;cout<<"right_bracket = "<<right_bracket<<endl;
		}
		cout<<"left bracket: "<<left_bracket_num<<"  right bracket: "<<right_bracket_num<<endl;
		if(left_bracket_num - right_bracket_num == 1) {cout<<"now is storing arg "<<arg_no<<endl;
			args[arg_no - 1].append(code->substr(pre_comma + 1, comma_pos - pre_comma - 1));
			
			pre_comma = comma_pos;
			arg_no++;
			left_bracket_num = 1;
			right_bracket_num = 0;
		}
		if(code->find_first_of(',', comma_pos + 1) == string::npos && arg_no == 3) {
			right_bracket = code->find_first_of(')', right_bracket + 1);
			args[arg_no - 1].append(code->substr(pre_comma + 1, right_bracket - pre_comma - 1));
			cout<<args[arg_no - 1]<<endl;
		}
	}	
	
	loperand->append(args[arg_pos - 1]);
	cout<<arg_pos<<"  "<<*loperand<<'\n';
	return true;
}

//创建溢出函数信息，格式：
//klee_detect_int(&左运算数, &右运算数, 位数, 错误类型)
bool create_ovf_func(string *code, string int_type, string *func, int &first_op, oi type, int fenhao) {
	string left_operand("");
	string right_operand("");
	
	
	int left = 0, right = 0;
	int left_bracket_num = 0;
	int in_left = 0, in_right = 0;
	int left_num = 0, right_num = 0;
	
	switch(type.type_no) {
		case UADD:
		case SADD:
			if(code->at(first_op + 1) == '+') {// ++
				if(isCharacter(code->at(first_op - 1)) || isNumber(code->at(first_op - 1)) || 
				   code->at(first_op - 1) == ']' || code->at(first_op - 1) == ')') {
					if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
						return false;
					}
					right_operand.append("1");
					first_op++;
				}
				else if(isCharacter(code->at(first_op + 2)) || isNumber(code->at(first_op + 2)) || 
				   code->at(first_op + 2) == '[' || code->at(first_op + 2) == '(' || code->at(first_op + 2) == '_') {
					if(!extract_operand(code, false, &left_operand, first_op + 1, fenhao)) {
						return false;
					}
					right_operand.append("1");
					first_op++;
				}
				else
					first_op++;
			}
			else {
				if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
					return false;
				}
				if(!extract_operand(code, false, &right_operand, first_op, fenhao)) {
					return false;
				}					
			}
			break;
		case USUB:
		case SSUB:
			if(code->at(first_op + 1) == '-') {// --
				if(isCharacter(code->at(first_op - 1)) || isNumber(code->at(first_op - 1)) || 
				   code->at(first_op - 1) == ']' || code->at(first_op - 1) == ')') {
					if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
						return false;
					}
					right_operand.append("1");
					first_op++;
				}
				else if(isCharacter(code->at(first_op + 2)) || isNumber(code->at(first_op + 2)) || 
				   code->at(first_op + 2) == '[' || code->at(first_op + 2) == '(') {
					if(!extract_operand(code, false, &left_operand, first_op + 1, fenhao)) {
						return false;
					}
					right_operand.append("1");
					first_op++;
				}
				else
					first_op++;
			}
			else {
				if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
					return false;
				}
				if(!extract_operand(code, false, &right_operand, first_op, fenhao)) {
					return false;
				}
			}
			break;
		case UMUL:
		case SMUL:
			if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
				return false;
			}
			if(!extract_operand(code, false, &right_operand, first_op, fenhao)) {
				return false;
			}
			break;
		case UDIV:
		case SDIV:
			if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
				return false;
			}
			if(!extract_operand(code, false, &right_operand, first_op, fenhao)) {
				return false;
			}
			break;
		case SHL:
		case LSHR:
		case ASHR:
		      if(first_op != string::npos) {
			      if(!extract_operand(code, true, &left_operand, first_op, fenhao)) {
				      return false;
			      }
			      if(!extract_operand(code, false, &right_operand, first_op + 1, fenhao)) {
				      return false;
			      }
		      }
		      break;
		case ARRAY:
			left = code->find_first_of('[', fenhao);
			right = code->find_first_of(']', left);
			if(left == string::npos || right == string::npos || left > right) {
				cout<<"can not find [] in code";
				func->clear();
				return false;
			}
			left_operand.append(code->substr(left + 1, right - left - 1));
			char ri[20];
			my_itoa(type.bit_or_size, ri);
			right_operand.append(ri);
			break;
		case SIZE:
			right_operand.append("0");
			if(!extract_func_arg(code, type.func_name, type.pos, &left_operand)) {
				cout<<"find error in create size func\n";
				return false;
			}
			break;
		default:break;
	}
	
	
	
	//eleminate '&'
	int site = 0;
	if((site = left_operand.find_first_of('&')) && site == left_operand.find_first_not_of(' ')) {
		left_operand = left_operand.substr(site + 1, left_operand.size() - site - 2);
	}
	
	//如果遇到了(__u32),放弃生成函数
	if(left_operand.find("(__u32)") != string::npos || right_operand.find("(__u32)") != string::npos) {
		cout<<"find (__u32), give up the operation.\n";
		return true;
	}
	
	char var[64];
	my_itoa(var_count, var);
	string var_num(var);
	var_count++;
	
	func->append(int_type);
	func->append("templ");
	func->append(var_num);
	func->append(" = ");
	func->append(left_operand);
	func->append(";");
	func->append(int_type);
	func->append("tempr");
	func->append(var_num);
	func->append(" = ");
	func->append(right_operand);
	func->append(";");
	
	func->append("klee_detect_int(&templ");
	func->append(var_num);
	func->append(", &tempr");
	func->append(var_num);
	func->append(", ");
		
	//create size
	char l[25];
	if(type.type_no == ARRAY)
		my_itoa(type.pos / 8, l);
	else
		my_itoa(type.bit_or_size / 8, l);
	func->append(l);
	
	//create type
	func->append(", ");
	
	char ty[4];
	my_itoa(type.type_no, ty);
	func->append(string(ty));
	func->append(");");
	return true;
}
//构造函数表达式
bool create_func(string *code, oi type, string *func) {
	string var_type("");
	string left_operand("");
	string right_operand("");
	
	if(func->size() != 0) {
		func->clear();
	}
	
	string int_type("");
	if(type.type_no == ARRAY)
		switch(type.pos) {
			case 32:
				int_type.append("uint32_t ");
				break;
			case 64:
				int_type.append("uint64_t ");
				break;
			case 16:
				int_type.append("uint16_t ");
				break;
			case 8:
				int_type.append("uint8_t ");
				break;
			default:return false;
		}
	else {
		switch(type.bit_or_size) {
			case 32:
				int_type.append("uint32_t ");
				break;
			case 64:
				int_type.append("uint64_t ");
				break;
			case 16:
				int_type.append("uint16_t ");
				break;
			case 8:
				int_type.append("uint8_t ");
				break;
			default:return false;
		}
	}
		
	int first_op = 0;
	int zhushi_pos = 0;
	int last_flag_pos = -1;
	int fenhao = 0;
	
	zhushi_pos = code->find("/*");
	while(code->find("klee_detect_int", last_flag_pos + 1) != string::npos) {
		last_flag_pos = code->find("klee_detect_int", last_flag_pos + 1);
	}
	cout<<zhushi_pos<<"  "<<last_flag_pos<<'\n';
	
	if(last_flag_pos != string::npos)
		fenhao = code->find_first_of(';', last_flag_pos + 1);
	
//	string orig_code = code->substr(fenhao + 1, code->size() - fenhao - 1);
//	cout<<orig_code;
	
	switch(type.type_no) {
		case UADD:
		case SADD:
			while((first_op = code->find_first_of('+', first_op + 1)) != string::npos) {
				if(zhushi_pos != string::npos && first_op > zhushi_pos) {
					break;
				}
				if(last_flag_pos != string::npos) {
					if(first_op < last_flag_pos)
						continue;
					if(type.type_no == UADD && code->find(", 0);") != string::npos)
						continue;
					if(type.type_no == SADD && code->find(", 1);") != string::npos)
						continue;
				}
				if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
					if(func->empty()) {
						cout<<"create add func failed\n";
						return false;
					}
					else
						continue;
				}
			}
			break;
		case USUB:
		case SSUB:
			first_op = 0;
			while((first_op = code->find_first_of('-', first_op + 1)) != string::npos) {
				if(zhushi_pos != string::npos && first_op > zhushi_pos) {
					break;
				}
				if(last_flag_pos != string::npos) {
					if(first_op < last_flag_pos)
						continue;
					if(type.type_no == USUB && code->find(", 2);") != string::npos)
						continue;
					if(type.type_no == SSUB && code->find(", 3);") != string::npos)
						continue;
				}
				
				while(code->at(first_op + 1) == '>') {
					first_op = code->find_first_of('-', first_op + 1);
					if(first_op == string::npos)
						break;
				}
				
				if(first_op != string::npos) {
					if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
						if(func->empty()) {
							cout<<"create sub func failed\n";
							return false;
						}
						else
							continue;
					}
				}
				else {
					break;
				}
			}
			break;
		case UMUL:
		case SMUL:
			while((first_op = code->find_first_of('*', first_op + 1)) != string::npos) {
				if(zhushi_pos != string::npos && first_op > zhushi_pos) {
					break;
				}
				if(last_flag_pos != string::npos) {
					if(first_op < last_flag_pos)
						continue;
					if(type.type_no == UMUL && code->find(", 4);") != string::npos)
						continue;
					if(type.type_no == SMUL && code->find(", 5);") != string::npos)
						continue;
				}
				
				if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
					if(func->empty()) {
						cout<<"create mul func failed\n";
						return false;
					}
					else		
						continue;
				}
			}
			break;
		case UDIV:
		case SDIV:
			while((first_op = code->find_first_of('/', first_op + 1)) != string::npos) {
				if(zhushi_pos != string::npos && first_op > zhushi_pos) {
					break;
				}
				if(last_flag_pos != string::npos) {
					if(first_op < last_flag_pos)
						continue;
					if(type.type_no == UDIV && code->find(", 6);") != string::npos)
						continue;
					if(type.type_no == SDIV && code->find(", 7);") != string::npos)
						continue;
				}
				
				if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
					cout<<"create div func failed\n";
					return false;
				}
			}
			break;
		case SHL:
			first_op = 0;
			while((first_op = code->find_first_of('<', first_op + 1)) != string::npos) {
				if(zhushi_pos != string::npos && first_op > zhushi_pos) {
					break;
				}
				if(last_flag_pos != string::npos) {
					if(first_op < last_flag_pos)
						continue;
					if(code->find(", 8);") != string::npos)
						continue;
				}
				
				while(code->at(first_op + 1) != '<') {
					first_op = code->find_first_of('<', first_op + 1);
					if(first_op == string::npos)
						break;
				}
				
				if(first_op != string::npos) {
					if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
						cout<<"create shl func failed\n";
						return false;
					}
					first_op ++;
				}
				else 
					break;
			}
			break;
		case LSHR:
		case ASHR:
			first_op = 0;
			while((first_op = code->find_first_of('>', first_op + 1)) != string::npos) {
				if(zhushi_pos != string::npos && first_op > zhushi_pos) {
					break;
				}
				if(last_flag_pos != string::npos) {
					if(first_op < last_flag_pos)
						continue;
					if(type.type_no == LSHR && code->find(", 9);") != string::npos)
						continue;
					if(type.type_no == ASHR && code->find(", 10);") != string::npos)
						continue;
				}
				
				while(code->at(first_op + 1) != '>') {
					first_op = code->find_first_of('>', first_op + 1);
					if(first_op == string::npos)
						break;
				}
				
				if(first_op != string::npos) {
					if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
						cout<<"create shr func failed\n";
						return false;
					}
					first_op ++;
				}
				else
					break;
			}
			break;
		case ARRAY:
			if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
				cout<<"create array func failed\n";
				return false;
			}
			break;
		case SIZE:
			if(!create_ovf_func(code, int_type, func, first_op, type, fenhao)) {
				cout<<"create size func failed\n";
				return false;
			}
			break;
		default:break;
	}
	
	return true;
}
//在源码中插桩
void insert_flag(li loc, FILE *fp) {
	if(!fp) {
		cout<<"file not exist!\n";
		exit(0);
	}
//	cout<<"insert flag\n";
	char line[1000];
	string *str, *prestr, *pre2str, *pre5[5];
	string bug(loc.bug.type);
	string command = "sed -i '";
	string function("");
	int count = 1;
	bool need_big_bracket = false;
	
	
	//create sed command
	char l[10];
	my_itoa(loc.line, l);
	command.append(l);
	command.append("c ");
	
	while(fgets(line, 1000, fp)) {
		//储存目标前7-3行字符串
		if(count == loc.line - 7) {
			int i = 0;
			for(; i<5; i++) {
				pre5[4 - i] = new string(line);
				fgets(line, 1000, fp);
				count++;
			}
		}
	  
		//存储目标行的前第二行
		if(count == loc.line - 2) {
			pre2str = new string(line);
		}
	  
		if(count == loc.line - 1) {
			prestr = new string(line);
			cout<<*prestr;
			
			//如果前一行是注释的尾部
			if(prestr->find("/*") == string::npos && prestr->find("*/") != string::npos) {
				if(pre2str->find("/*") != string::npos) {
					if(pre2str->find_first_not_of('\t') == pre2str->find("/*")) {
						prestr = pre5[0];
					}
					else {
						prestr = pre2str;
					}
				}
				else {//前第二行不是注释的开始，而是注释的一部分
					int i = 0;
					//往前寻找注释的开始
					for(; i < 5; i++) {
						//找到注释的开始
						if(pre5[i]->find("/*") != string::npos) {
							break;
						}
					}
					if(i != 5) {//找到了注释的开始
						if(pre5[i]->find_first_not_of('\t') == pre5[i]->find("/*") && i != 0) {
							prestr = pre5[i - 1];
						}
						else {
							prestr = pre5[i];
						}
					}
				}
			}
			
			//如果当前行为注释，则将pre2str代替prestr
			if(prestr->find_first_not_of('\t') == prestr->find("/*")) {
				prestr = pre2str;
			}
			else{//如果该行没有结束，则直接推出
				if(prestr->find_first_of('+')!= string::npos && prestr->find_first_of(';', prestr->find_first_of('+')) == string::npos) {
					break;
				}
			}
			
			//
			
			if(prestr->size() > 1 && prestr->at(prestr->size() - 1) == '\n') {
				if(prestr->at(prestr->size() - 2) != ';' && prestr->at(prestr->size() - 2) != ')' && 
				   prestr->at(prestr->size() - 2) != '{' && prestr->at(prestr->size() - 2) != '}' && 
				   prestr->find("*/") == string::npos) {cout<<"b\n";
					int zs = prestr->find("/*");
					if(zs != string::npos ) {
						int ch = prestr->substr(0, zs - 1).find_last_not_of(' ');
						if(ch == string::npos)
							break;
						if(prestr->at(ch) != ';' && prestr->at(ch) != ')' && prestr->at(ch) != '{' &&
						   prestr->at(ch) != '}')
							break;
					}
					break;
				}
			}
			
			if(prestr->size() > 1) {
				if(prestr->find("if (") != string::npos || prestr->find("for (") != string::npos || prestr->find("while (") != string::npos ||
				  prestr->find("if(") != string::npos || prestr->find("for(") != string::npos || prestr->find("while(") != string::npos || 
				  prestr->at(prestr->size() - 2) == ')' || prestr->find("else") != string::npos) {
					if(prestr->find_first_of('{') == string::npos) {
						int match_num = 0;
						for(int i = 0; i < prestr->size(); i++) {
							if(prestr->at(i) == '(')
								match_num++;
							if(prestr->at(i) == ')')
								match_num--;
						}
						if(match_num <=0)
							need_big_bracket = true;
						else
							break;
					}
				}
			}
		}
		if(loc.line == count) {
			str = new string(line);
//			cout<<loc.line<<"    "<<line<<"\n";
//			cout<<*prestr;
			cout<<*str;
		
			//字符数量太少，可能是行数出现了问题
			if(str->size() <= 4) {
				break;
			}

			if(str->find("klee_detect_int") != string::npos) {
				//create size
				char l[25];
				if(loc.bug.type_no == ARRAY)
					my_itoa(loc.bug.pos / 8, l);
				else
					my_itoa(loc.bug.bit_or_size / 8, l);
				string feature(", ");
				feature.append(l);
				feature.append(", ");
				char ty[4];
				my_itoa(loc.bug.type_no, ty);
				feature.append(string(ty));
				feature.append(");");
							
				if(str->find(feature.c_str()) != string::npos) {
					break;
				}
			}
			
			if(str->find("else") != string::npos) {
				if(str->substr(0, str->find("else")).find_first_of('}') == string::npos)
					 break;
			}
			
			if(str->at(str->size() - 1) == '\n') {
				if(str->at(str->size() - 2) != ';' && str->at(str->size() - 2) != '{' && 
				  str->at(str->size() - 2) != '}' && str->find("*/") == string::npos && str->at(str->size() - 2) != ')') {
					break;		
				}
				else if(str->at(str->size() - 2) == ')' && str->find("if (") == string::npos && str->find("while (") == string::npos) {
					break;
				}
				else if(str->find("if (") != string::npos || str->find("while (") != string::npos) {
					if(need_big_bracket) {
						break;
					}
				}
			}
				
			
			int first = str->find_first_not_of('\t');
			int second = str->find_first_not_of(' ', first);
			if(str->at(second) == '+' || str->at(second) == '-' ||
			   str->at(second) == ',' || str->at(second) == '*' ||
			   str->at(second) == '=' || str->at(second) == '/' ||
			   str->at(second) == '[' || str->at(second) == '<' ||
			   str->at(second) == '>') {
				break;
			}
			
			if(str->find_first_of('\n') != str->find_last_of('\n')) {
				break;
			}
			
			if(str->find("sprintf") != string::npos || str->find("snprintf") != string::npos) {
				break;
			}
			
			if(!create_func(str, loc.bug, &function) || function.empty()) {
				cout<<"create function failed\n";
				break;
			}

			//判断是否需要将插入代码放在源码的后面
			if(str->find("cputime_t rtime, utime = p->utime, total = utime + p->stime;") != string::npos) {
				break;
			//	need_reverse = true;
			}
			//else {
			//	need_reverse = false;
			//}
			
			string final("");
			if(need_big_bracket) {
				final.append("{");
			}

			//modify str
			int sq_pos = 0;
			while((sq_pos = str->find_first_of('\'', sq_pos)) != string::npos) {
				str->insert(sq_pos, "\'\\'");
				sq_pos += 4;
			}

			if((sq_pos = str->find("\\n")) != string::npos) {
				str->insert(sq_pos, "\\");
			}

			//如果需要将插入代码放在源码的后面，则先添加源码字符串
			//if(need_reverse) {
			//	final.append(*str);
			//}
			
			//output overflow information
			char var[64];
			my_itoa(var_count, var);
			string var_num(var);
			my_itoa(loc.line, var);
			string loc_line(var);
			
			final.append("char info");
			final.append(var_num);
			final.append("[] = \"");
			final.append(loc.path);
			final.append(":");
			final.append(loc_line);
			final.append("\";");
			//final.append("klee_message(info");
			final.append(var_num);
			final.append(");");
			
			final.append(function);
			
			//modify str
			//int sq_pos = 0;
			//while((sq_pos = str->find_first_of('\'', sq_pos)) != string::npos) {
			//	str->insert(sq_pos, "\'\\'");
			//	sq_pos += 4;
			//}
			
			//if(!need_reverse) {
				final.append(*str);
			//}
			
			if(need_big_bracket) {
				final.insert(final.size() - 1, 1, '}');
				need_big_bracket = false;
			}

			//将need_reverse重置
			need_reverse = false;
			
			//create command
			command.append(final);
			command.append("' ");
			command.append(loc.path);
			cout<<command<<'\n';
			system(command.c_str());
			break;
		}
		count++;
	}
}

int main() {
	FILE *fkint;//指向pintck.txt
	FILE *fsrc;//指向需要插桩的源码文件
	
	string bug_prefix = "bug";//整数溢出类型信息的前缀
	string loc_prefix = " - ";//整数溢出位置信息的前缀
	
	li location;//用来存储整数溢出错误的位置信息
	bool pre_is_bug = false;//用来标识前一个读取的内容是否是错误信息
	
	list<li> locations;//用来存储所有位置信息的链表
	list<string> files;//用来储存所有源码文件信息的链表
	
	//打开pintck.txt
	if((fkint = fopen("pintck.txt", "r")) == NULL) {
		cout<<"pintck.txt load failed.\n";
		return 0;
	}
	
	//存储文件中一行内容的缓冲区
	char lkint[200];
	string *skint;
	
	bool location_is_new = false;//标识当前的位置信息是否是新的
//	bool fsrc_is_closed = true;
	
	//循环地从文件中一行一行读取内容
	while(fgets(lkint, 200, fkint)) {
		//cout<<lkint;
		skint = new string(lkint);//将读取的一行内容转换成string类型
		
		//在读取出的一行内容中找到了字符串“bug”，表示该行信息是整数溢出类型信息
		if(skint->find("bug") != string::npos) {
			//调用extract_ovf_info提取出错误类型信息
			extract_ovf_info(skint, location.bug);
			
			//如果提取失败则直接跳过本次错误
			if(location.bug.type.empty()) {
				continue;
			}
			//将pre_is_bug标为true，表示本次读取出的是一个bug信息
			pre_is_bug = true;
		}
		//在读出的一行内容中找到了字符串“ - ”，表示该行信息是位置信息
		if(skint->find(" - ") != string::npos && pre_is_bug) {
			
			//因为目前对include中的文件进行插桩会导致编译无法通过，所以跳过头文件
			if(skint->find("include/") != string::npos) {
				pre_is_bug = false;
				continue;
			}
			
			//调用extract_loc_info提取出位置信息
			extract_loc_info(skint, location);
			//location.push_back(locations);
			//表示当前提取出的信息不是bug信息
			pre_is_bug = false;
			//表示当前的位置信息是新的
			location_is_new = true;
		}    
		//cout<<*skint;
		//如果当前的位置信息是新提取出来的
		if(location_is_new == true) {
			//将location_is_new置为false，为下一次做准备
			location_is_new = false;
			
			//将位置信息中的文件信息存储到文件信息链表中 
			if(files.size() == 0) {//当链表为空时直接插入
				files.push_back(location.path);
			}
			else {//否则查询是否文件信息已存在，若已存在则不插入
				list<string>::iterator p = files.begin();
				for(; p != files.end(); p++) {
					if(location.path == string(*p)) {
						break;
					}
				}
				if(p == files.end())
					files.push_back(location.path);
			}
			
			//如果位置信息链表为空则直接将新得到的位置信息插入链表中
			if(locations.size() == 0) {
				locations.push_back(location);
			}
			
			else if(location.path == locations.front().path) {
				list<li>::iterator i = locations.begin();
				for(; i != locations.end(); i++) {
					if(location.line == li(*i).line && location.bug.type_no == li(*i).bug.type_no && 
					  location.bug.bit_or_size == li(*i).bug.bit_or_size) 
						break;
				}
				if(i == locations.end())
					locations.push_back(location);
				else	
					continue;
			}
			
			//file changed
			else {//cout<<locations.size()<<'\n';				
				
				locations.clear();
				locations.push_back(location);
			}
			
			//open target file and insert flag
			if((fsrc = fopen(location.path.c_str(), "r+")) == NULL) {
				cout<<"src file is not existed!\n";
			}
			
			insert_flag(location, fsrc);	
			
			fclose(fsrc);
			
			cout<<"--------------------------------------------\n";
		}		
	}
	
	cout<<"files : "<<files.size()<<endl;
	list<string>::iterator p = files.begin();
	FILE *fhead;
	char first_line[200];
	for(; p != files.end(); p++) {
		if((fhead = fopen(string(*p).c_str(), "r")) == NULL) {
			cout<<"file not existed\n";
			continue;
		}
		fgets(first_line, 200, fhead);
		string sed_command("sed -i '1c #include <s2e/s2e.h>' ");
		sed_command.append(string(*p));
		cout<<sed_command<<'\n';
		//system(sed_command.c_str());
		sed_command.clear();
		
		sed_command.append("sed -i '1a ");
		string ff(first_line);
		if(ff.size() == 1 && ff.at(0) == '\n') {
			cout<<"the first line is empty\n";
			sed_command.clear();
			continue;
		}
		while(ff.find_first_of('\'') != string::npos) {
			ff = ff.erase(ff.find_first_of('\''), 1);
		}
		
		sed_command.append(ff.substr(0, ff.size() - 1));
		sed_command.append("' ");
		sed_command.append(string(*p));
		cout<<sed_command<<'\n';
		//system(sed_command.c_str());
	}
	
	
	return fclose(fkint);
  
}
