#pragma once
class Program;

class AddressGenerator
{
public:
	AddressGenerator() :
		last_heap_address(0),
		last_heap_obj_size(0){}

	Address generateStackAddress(/*const std::string &fun_name,*/ int size);
	Address generateHeapAddress(int size);
	void reset();
	void resetStack(const Address &last_addr, const int last_size);
private:
	std::map<std::string, std::pair<Address, int> > stack_addresses;
	Address last_stack_address;
	int last_stack_obj_size;
	Address last_heap_address;
	int last_heap_obj_size;
};

enum class LeftValueKind {
	None,
	StructMember,
	Variable
};

/**
 * Данные о том, какому объекту будет выполнено присваивание.
 *	var_name - имя переменной;
 *	fields_name_chain - цепочка обращений к полям стр-ры, если это происходит;
 *		например, list.head->next->next 
 *		в этом случае в поле var_name будет помещено list,
 *		а в fields_name_chain будут помещены head, next, next
 *		стек использован потому, что в конченом счете присваивание будет второму полю next
 *		и обрабатывать эти обращения нужно начиная с начала, т.е., в данном случае, с head
 */
struct SymbolData
{
	std::string var_name;
	std::stack<std::string> fields_name_chain;
};

struct HeapObjectConnections
{
	std::shared_ptr<Object> current_object;
	std::vector<std::shared_ptr<Object> > linked_objects;
	int current_considered_object;
	std::shared_ptr<HeapObjectConnections> object_link;
	HeapObjectConnections(const std::shared_ptr<Object> cur_obj = nullptr,
		int cur_consid_obj = 0,
		std::shared_ptr<HeapObjectConnections> obj_conn = nullptr) :
		current_object(cur_obj),
		current_considered_object(cur_consid_obj),
		object_link(obj_conn) {}
};

struct FindMemoryLeakResult
{
	Address heap_obj_address;
	std::pair<CXCursor, std::string> allocate_operator;
	std::vector<std::tuple<CXCursor, std::string, std::string> > operators;
	CXCursor last_operator;
	bool leak_op_find;
	FindMemoryLeakResult() : last_operator(clang_getNullCursor()),
		leak_op_find(false)
	{}
};

enum class AssignTo {
	None = 0x0,
	Parameter,
	Variable
};

struct CallData
{
	CXCursor call_expr;
	AssignTo assign_to;
	SymbolData symbol_data;
	int arg_num;
	std::string fun_name;
	std::string evaluate_fun_name;
};

class SymbolEvaluator
{
public:
	SymbolEvaluator() {}

	void checkForMemoryLeaks(const std::shared_ptr<Program> program, const Paths &paths);
	void checkForMemoryLeaks_(const std::shared_ptr<Program> program, const PathsId &paths);

	void setPathSections(const std::map<int, PathSection> &path_sec) { path_sections = path_sec; }

	bool isObjectLeak(const FunDeclarations &fun_decls, const Object &heap_obj);

private:
	AddressGenerator address_generator;
	std::shared_ptr<Program> current_program;
	FunDeclarations analyze_functions;
	UserTypeDeclarations analyze_user_types;
	std::map<int, PathSection> path_sections;
	Heap heap;

	void evaluatePath(const std::vector<PathSection> &path, const bool find_memory_leak, std::vector<FindMemoryLeakResult> &result);
	//void evaluatePath_(const std::vector<int> &path, const bool find_memory_leak, std::vector<FindMemoryLeakResult> &result);
	std::set<std::shared_ptr<Object> > getLeakedObjects();
	bool hasAccessFromProgram(const std::shared_ptr<HeapObjectConnections> object,
		std::set<std::shared_ptr<Object> > &leaked_objects,
		std::set<std::shared_ptr<Object> > &has_access);
	std::stack<CallData> getCallData(const CXCursor &call_stmt, const std::string &context_name);
	SymbolData getSymbolData(const std::vector<CXCursor> &op_cursors, int &offset);
	const std::shared_ptr<Object> getSymbolObject(SymbolData &symbol_data, const std::string &context_name);
	void printResult(const std::vector<FindMemoryLeakResult> &find_memory_leak_result);
};

