#include "stdafx.h"
#include "Program.h"
#include "Path.h"
#include "SymbolEvaluator.h"

void parseCursor(const CXCursor &cursor, std::vector<CXCursor> &cursors);
std::string getTypeSpelling(const CXType &type);
bool isCursorTypePointer(CXType type, int &level_indirection);
bool isTypeConst(const CXType &type);
bool isInternalType(const CXType &type);
void printFunctionMemoryState(const Function &function);
void printHeapState(const Heap &heap);
bool isOperatorAssign(const CXCursor &op_cursor);
void tokenize(CXCursor cursor, CXTranslationUnit &unit, CXToken **tokens, unsigned int &num_tokens);
std::string getFilePathString(const CXCursor &cursor);

Address AddressGenerator::generateStackAddress(/*const std::string &fun_name,*/ int size)
{
	//if (stack_addresses.count(fun_name) == 0) {
	//	stack_addresses.insert(std::make_pair(fun_name, std::make_pair(Address(1), size)));
	//	return stack_addresses.at(fun_name).first;
	//} else {
	//	Address new_address(stack_addresses.at(fun_name).first.getAddressValue() + stack_addresses.at(fun_name).second);
	//	stack_addresses.at(fun_name).first = new_address;
	//	stack_addresses.at(fun_name).second = size;
	//	return new_address;
	//}
	if (last_stack_address.getAddressValue() == 0) {
		last_stack_address.setAddressValue(1);
		last_stack_obj_size = size;
	} else {
		last_stack_address.setAddressValue(last_stack_address.getAddressValue() + last_stack_obj_size);
		last_stack_obj_size = size;
	}
	return last_stack_address;
}

constexpr int HEAP_OFFSET = 1000;

Address AddressGenerator::generateHeapAddress(int size)
{
	if (last_heap_address.getAddressValue() == 0) {
		last_heap_address.setAddressValue(1 + HEAP_OFFSET);
		last_heap_obj_size = size;
	} else {
		last_heap_address.setAddressValue(last_heap_address.getAddressValue() + last_heap_obj_size);
		last_heap_obj_size = size;
	}
		return last_heap_address;
}

void AddressGenerator::reset()
{
	//stack_addresses.clear();
	last_stack_address.setAddressValue(0);
	last_stack_obj_size = 0;
	last_heap_address.setAddressValue(0);
	last_heap_obj_size = 0;
}

void AddressGenerator::resetStack(/*const std::string &fun_name*/const Address &last_addr, const int last_size)
{
	//if (stack_addresses.count(fun_name) > 0) {
	//	stack_addresses.at(fun_name).first.setAddressValue(0);
	//	stack_addresses.at(fun_name).second = 0;
	//}
	last_stack_address.setAddressValue(last_addr.getAddressValue());
	last_stack_obj_size = last_size;
}

void SymbolEvaluator::checkForMemoryLeaks(const std::shared_ptr<Program> program, const Paths &paths)
{
	current_program = program;
	analyze_functions = current_program->cloneFunctions();
	analyze_user_types = current_program->cloneUserTypes();

	for (const auto &path : paths) {
		std::vector<FindMemoryLeakResult> result;
		//std::vector<CXCursor> result;
		
		//Перед началом выполнения пути очищаем стек ф-ций.
		for (auto &[name, fun] : analyze_functions) {
			fun->clearStack();
			current_program->clearStack(name);
		}
		evaluatePath(path, false, result);
		for (auto &[name, fun] : analyze_functions) {
			fun->clearStack();
			current_program->clearStack(name);
		}
		/*for (const auto &[name, fun] : analyze_functions.back()) {
			printFunctionMemoryState(*fun);
		}
		printHeapState(heaps.back());*/
		//std::cout << "*********************First pass ended***********************\n";
		//if (heaps.back().isEmpty()) {
		//	continue;
		//}
		for (const auto &obj : heap.getHeap()) {
			result.emplace_back();
			result.back().heap_obj_address = obj->getAddress();
		}
		heap.clear();
		evaluatePath(path, true, result);
		printResult(result);
		std::cout << "End leak find\n";
	}

	current_program.reset();
}

void SymbolEvaluator::checkForMemoryLeaks_(const std::shared_ptr<Program> program, const PathsId &paths)
{
	current_program = program;
	analyze_functions = current_program->cloneFunctions();
	analyze_user_types = current_program->cloneUserTypes();

	for (const auto &path : paths) {
		std::vector<FindMemoryLeakResult> result;
		//std::vector<CXCursor> result;

		//Перед началом выполнения пути очищаем стек ф-ций.
		for (auto &[name, fun] : analyze_functions) {
			fun->clearStack();
		}
		//evaluatePath_(path, false, result);
		for (auto &[name, fun] : analyze_functions) {
			fun->clearStack();
		}
		/*for (const auto &[name, fun] : analyze_functions.back()) {
			printFunctionMemoryState(*fun);
		}
		printHeapState(heaps.back());*/
		//std::cout << "*********************First pass ended***********************\n";
		//if (heaps.back().isEmpty()) {
		//	continue;
		//}
		for (const auto &obj : heap.getHeap()) {
			result.emplace_back();
			result.back().heap_obj_address = obj->getAddress();
		}
		heap.clear();
		//evaluatePath_(path, true, result);
		printResult(result);
		std::cout << "End leak find\n";
	}

	current_program.reset();
}

/**
 * Символьное выполнение одного из путей программы.
 * Первый параметр - путь программы.
 * Второй параметр - флаг, для того чтобы дополнительно выполнять поиск утечки памяти.
 * Третий параметр - структура, в которую будет помещаться результат поиска утечки.
 */
void SymbolEvaluator::evaluatePath(const std::vector<PathSection> &path, const bool find_memory_leak, std::vector<FindMemoryLeakResult> &result)
{
	//Сбрасываем генератор адресов.
	address_generator.reset();
	//Данный стек контекстов нужен для того, чтобы при выходе из ф-ции, очистить ее от размещенных,
	//в процессе ее выполнения, в ее стеке, переменных и объектов
	std::stack<std::string, std::vector<std::string> > context_calls;
	std::stack<CallData> call_data_stack;
	std::string current_context;
	//Возвращаемое из ф-ции значение помещается в этот объект
	std::shared_ptr<Object> returned_object = std::make_shared<Object>();
	//Для проверки, яв-ся ли некоторый объект указателем.
	int dummy{0};
	for (const auto &seg : path) {

		//Переходы между ф-циями и очистка стека ф-ции, в случае выхода из нее.
		//Если при переходе из одной ф-ции в другую ф-ция завершила свою работу,
		//то чистим стек. Плюс обработка возвращаемого из функции значения.
		current_context = seg.context_name;
		if (context_calls.size() > 1) {
			if ((current_context != context_calls.top())
				&& (current_context == (*(&context_calls.top() - 1)))) {
				auto last_variable = analyze_functions.at(context_calls.top())->clearStack();
				current_program->clearStack(context_calls.top());
				//address_generator.resetStack(context_calls.top());
				context_calls.pop();
				address_generator.resetStack(last_variable.getAddress(), last_variable.getType().getSize());
			} else if (((*(&seg - 1)).is_end)) {
				if (!call_data_stack.empty()) {
					std::shared_ptr<Object> appropriated_object;
					std::string appropriated_name;
					if (call_data_stack.top().assign_to == AssignTo::Parameter) {
						auto var = analyze_functions.at(call_data_stack.top().fun_name)->getParameterByNum(call_data_stack.top().arg_num).argument;
						auto variable = std::make_shared<Variable>();
						*variable = var;
						//variable->setAddress(address_generator.generateStackAddress(call_data_stack.top().fun_name, var.getType().getSize()));
						variable->setAddress(address_generator.generateStackAddress(var.getType().getSize()));
						//appropriated_object = std::make_shared<Object>(variable->getAddress(), variable->getType());
						appropriated_object = std::make_shared<Object>(variable->getAddress(), variable->getType(), call_data_stack.top().fun_name);
						analyze_functions.at(call_data_stack.top().fun_name)->addVariable(variable);
						appropriated_name = variable->getName();
					} else if (call_data_stack.top().assign_to == AssignTo::Variable) {
						SymbolData symbol_data_copy = call_data_stack.top().symbol_data;
						while (!symbol_data_copy.fields_name_chain.empty()) {
							appropriated_name.insert(0, ".(->)" + symbol_data_copy.fields_name_chain.top());
							symbol_data_copy.fields_name_chain.pop();
						}
						appropriated_name.insert(0, symbol_data_copy.var_name);
						appropriated_object = getSymbolObject(call_data_stack.top().symbol_data, current_context);
					}
					
					if (!returned_object->getType().isPointer() && current_program->hasUserType(returned_object->getType().getTypeName())) {
						for (const auto &field : current_program->getUserTypeByName(returned_object->getType().getTypeName())->getFields()) {
							appropriated_object->addPointerField(field);
							appropriated_object->getPtrFieldByName(field.getName())->setAddressVal(returned_object->getPtrFieldByName(field.getName())->getAddressVal());
						}
					} else if (returned_object->getType().isPointer()) {
						appropriated_object->setAddressVal(returned_object->getAddressVal());
					}
					if (find_memory_leak) {
						std::set<std::shared_ptr<Object> > leaked_objects = getLeakedObjects();
						for (auto &f_mem_res : result) {
							if (f_mem_res.heap_obj_address.getAddressValue() == returned_object->getAddressVal().getAddressValue()) {
								f_mem_res.operators.push_back({ call_data_stack.top().call_expr , call_data_stack.top().evaluate_fun_name, appropriated_name });
							}
						}
					}
					if (call_data_stack.top().assign_to == AssignTo::Parameter) {
						//analyze_functions.at(call_data_stack.top().fun_name)->addObjToStack(appropriated_object);
						current_program->addObjToStack(appropriated_object);
					}
					call_data_stack.pop();
					returned_object = std::make_shared<Object>();
				}
				//analyze_functions.at((*(&seg - 1)).context_name)->clearStack();
				auto last_variable = analyze_functions.at((*(&seg - 1)).context_name)->clearStack();
				current_program->clearStack((*(&seg - 1)).context_name);
				//address_generator.resetStack((*(&seg - 1)).context_name);
				context_calls.pop();
				address_generator.resetStack(last_variable.getAddress(), last_variable.getType().getSize());
				context_calls.push(current_context);
			} else if (current_context != context_calls.top()) {
				context_calls.push(current_context);
			}
		} else if (context_calls.empty() || (context_calls.top() != current_context)) {
			context_calls.push(current_context);
		}
		
		//Выполнение операторов
		//Обрабатываются следующие операторы:
		//	- оператор объявления(указателя либо стр-ры, среди полей которой могут быть указатели;
		//	- опертор объявления с присваиванием;
		//	- оператор присваивания:
		//		* указателя указателю;
		//		* указателя полю стр-ры
		//		* результат операции new указателю;
		//		* результат операции new полю стр-ры(любой вложенности);
		//		* поля стр-ры(любой вложенности) указателю;
		//		* поля стр-ры(любой вложенности) полю стр-ры(любой вложенности);
		for (decltype(seg.operators.size()) i = 0; i < seg.operators.size(); ++i) {
			auto kind = clang_getCursorKind(seg.operators.at(i));
			//Смещение, для того, чтобы по завершении обработки оператора, перейти к следующему, с учетом
			//всех, находящихся между двумя операторами, курсоров.
			size_t offset{0};
			//оператор объявления
			if (kind == CXCursorKind::CXCursor_DeclStmt) {
				std::vector<CXCursor> decl_cursors;
				parseCursor(seg.operators.at(i), decl_cursors);
				offset = decl_cursors.size();
				//проверка на то, вып-ся ли присваивание в объявлении
				if (isOperatorAssign(seg.operators.at(i))) {
					//запоминание имени объвленной переменной, которой будет присвоено значение
					std::string decled_var;
					for (decltype(decl_cursors.size()) j = 0; j < decl_cursors.size(); ++j) {
						auto op_kind = clang_getCursorKind(decl_cursors.at(j));
						auto op_type = clang_getCursorType(decl_cursors.at(j));
						if (op_kind == CXCursorKind::CXCursor_VarDecl) {
							if (!isCursorTypePointer(op_type, dummy)) {
								break;
							}
							//Получаем об объявляемой переменной необходимую информацию:
							//	- имя
							//	- тип
							//	- указатель или нет (с учетом уровня косвенности)
							//	- const или нет
							//	- размер типа
							auto var_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(decl_cursors.at(j))));
							auto type_name = getTypeSpelling(op_type);
							int level_indirection{0};
							auto is_ptr = isCursorTypePointer(op_type, level_indirection);
							auto is_const = isTypeConst(op_type);
							int type_size = is_ptr ? 1 : current_program->getTypeSize(type_name);
							if (is_ptr) {
								decled_var = var_name;
								//Генерируем адрес в стеке и создаем переменную и объект в стеке, добавляя их в программу
								//Address new_address = address_generator.generateStackAddress(seg.context_name, type_size);
								auto new_address = address_generator.generateStackAddress(type_size);
								Type type{type_name, is_const, is_ptr, level_indirection, type_size};
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								auto new_stack_obj = std::make_shared<Object>(new_address, type, seg.context_name);
								analyze_functions.at(seg.context_name)->addVariable(new_var);
								//analyze_functions.at(seg.context_name)->addObjToStack(new_stack_obj);
								current_program->addObjToStack(new_stack_obj);
							} else if (current_program->hasUserType(type_name)) {
								decled_var = var_name;
								//Address new_address = address_generator.generateStackAddress(seg.context_name, type_size);
								auto new_address = address_generator.generateStackAddress(type_size);
								Type type{type_name, is_const, is_ptr, level_indirection, type_size};
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								auto new_stack_obj = std::make_shared<Object>(new_address, type, seg.context_name);
								//Если переменная яв-ся объектом пользовательского типа, то добавляем в него необходимые поля,
								//в соответствии описанием пользовательского типа.
								for (const auto &field : current_program->getUserTypeByName(type_name)->getFields()) {
									new_stack_obj->addPointerField(field);
								}
								analyze_functions.at(seg.context_name)->addVariable(new_var);
								//analyze_functions.at(seg.context_name)->addObjToStack(new_stack_obj);
								current_program->addObjToStack(new_stack_obj);
							}
						} else if (op_kind == CXCursorKind::CXCursor_DeclRefExpr) {
							if (!isCursorTypePointer(op_type, dummy)) {
								break;
							}
							auto assigned_var_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(decl_cursors.at(j))));
							//Получаем приcваиваемый объект.
							auto assigned_var = analyze_functions.at(seg.context_name)->getVarByName(assigned_var_name);
							//auto assigned_obj = analyze_functions.at(seg.context_name)->getObjByAddress(assigned_var->getAddress());
							auto assigned_obj = current_program->getObjByAddress(assigned_var->getAddress());
							//Получаем объект, которому будет выполняться присваивание.
							auto appropriated_var = analyze_functions.at(seg.context_name)->getVarByName(decled_var);
							//auto appropriated_obj = analyze_functions.at(seg.context_name)->getObjByAddress(appropriated_var->getAddress());
							auto appropriated_obj = current_program->getObjByAddress(appropriated_var->getAddress());

							//Т.к. рассматривается присваивание только указателей, то помещаем адрес, который хранится в присваиваемом объекте,
							//объекту которому вып-ся присваивание.
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());

							//Режим поиска утечек памяти
							if (find_memory_leak) {
								//Перебираются все объекты кучи, которые были потеряны
								for (auto &f_mem_res : result) {
									//Если в объявленный указатель помещается потерянный адрес, то записываем этот оператор как присутствующий
									//в последовательности операторов использованных перед возникновением утечки
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({seg.operators.at(i), assigned_var->getName(), appropriated_var->getName() });
										break;
									}
								}
							}
						} else if (op_kind == CXCursorKind::CXCursor_CXXNewExpr) {
							//Получаем объект, которому будет выполняться присваивание.
							auto appropriated_var = analyze_functions.at(seg.context_name)->getVarByName(decled_var);
							//auto appropriated_obj = analyze_functions.at(seg.context_name)->getObjByAddress(appropriated_var->getAddress());
							auto appropriated_obj = current_program->getObjByAddress(appropriated_var->getAddress());

							//Получаем размер объекта, который будет размещен в куче. Для этого проверяем уровень косвенности указателя, если он равен 1, то в кучу
							//будет размещен объект, если больше 1, значит в кучу будет размещен указатель.
							int type_size = (appropriated_obj->getType().isPointer() && (appropriated_obj->getType().getLevelIndirection() > 1)) ? 1 : current_program->getTypeSize(appropriated_var->getType().getTypeName());
							//Генерируем адрес в куче.
							auto new_heap_address = address_generator.generateHeapAddress(type_size);
							//Присваиваем его.
							appropriated_obj->setAddressVal(new_heap_address);
							//Создаем объект в куче, в соответствии с параметрами объекта указателя и добавляем поля, если это объект пользовательского типа.
							Type heap_obj_type{appropriated_obj->getType().getTypeName(), appropriated_obj->getType().isConst(), ((appropriated_obj->getType().getLevelIndirection() - 1) > 0), appropriated_obj->getType().getLevelIndirection() - 1, type_size};
							auto heap_object = std::make_shared<Object>(new_heap_address, heap_obj_type, HEAP_CONTEXT);
							if (!heap_object->getType().isPointer() && current_program->hasUserType(heap_object->getType().getTypeName())) {
								for (const auto &field : current_program->getUserTypeByName(heap_object->getType().getTypeName())->getFields()) {
									heap_object->addPointerField(field);
								}
							}
							//Добавляем объект в кучу.
							heap.addObject(heap_object);

							//Режим поиска утечек памяти
							if (find_memory_leak) {
								for (auto &f_mem_res : result) {
									//Если объявленному указателю присваивается адрес объекта размещаемого в куче оператором new,
									//который был утерян, то фиксируем этот оператор, как тот в котором впервые был размещен объект
									if (f_mem_res.heap_obj_address.getAddressValue() == heap_object->getAddress().getAddressValue()) {
										f_mem_res.allocate_operator = std::make_pair(seg.operators.at(i), appropriated_var->getName());
										break;
									}
								}
							}
						} else if (op_kind == CXCursorKind::CXCursor_CXXNullPtrLiteralExpr) {
							//Если nullptr.
							//Получаем объект, которому будет выполняться присваивание.
							auto appropriated_var = analyze_functions.at(seg.context_name)->getVarByName(decled_var);
							//auto appropriated_obj = analyze_functions.at(seg.context_name)->getObjByAddress(appropriated_var->getAddress());
							auto appropriated_obj = current_program->getObjByAddress(appropriated_var->getAddress());

							appropriated_obj->setAddressVal(Address(0));
						} else if (op_kind == CXCursorKind::CXCursor_MemberRefExpr) {
							if (!isCursorTypePointer(op_type, dummy)) {
								break;
							}
							//Парсинг выражения обращения к полю стр-ры.
							SymbolData assign_data;
							std::string assigned_var_name;
							int field_access_count{0};
							for (decltype(decl_cursors.size()) k = j; k < decl_cursors.size(); ++k, ++j, ++field_access_count) {
								auto kind = clang_getCursorKind(decl_cursors.at(k));
								auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(decl_cursors.at(k))));
								if (kind == CXCursorKind::CXCursor_MemberRefExpr) {
									assign_data.fields_name_chain.push(name);
									if (find_memory_leak) {
										//Для красивого выведения на экран присваивания полю стр-ры.
										assigned_var_name.insert(0, ".(->)" + name);
										field_access_count = 0;
									}
								} else if (kind == CXCursorKind::CXCursor_DeclRefExpr) {
									assign_data.var_name = name;
									if (find_memory_leak) {
										//Для красивого выведения на экран присваивания полю стр-ры.
										assigned_var_name.insert(0, name);
										field_access_count = 0;
									}
									break;
								}
							}

							//Получение присваиваемого объекта.
							auto assigned_obj = getSymbolObject(assign_data, seg.context_name);

							//Получение итогового объкта, которму будет выполнено присваивание.
							auto appropriated_var = analyze_functions.at(seg.context_name)->getVarByName(decled_var);
							//auto appropriated_obj = analyze_functions.at(seg.context_name)->getObjByAddress(appropriated_var->getAddress());
							auto appropriated_obj = current_program->getObjByAddress(appropriated_var->getAddress());
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());

							//Режим поиска утечек памяти
							if (find_memory_leak) {
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({ seg.operators.at(i), assigned_var_name, appropriated_var->getName() });
										break;
									}
								}
							}
						}
					}
				/*	printFunctionMemoryState(*analyze_functions.back().at(seg.context_name));
					printHeapState(heaps.back());*/
				} else {
					//Обычное объявление, без присваивания. В рез-те будут размещены переменная и объект в стек ф-ции.
					for (const auto &op : decl_cursors) {
						auto op_kind = clang_getCursorKind(op);
						auto op_type = clang_getCursorType(op);
						if (op_kind == CXCursorKind::CXCursor_VarDecl) {
							if (!isCursorTypePointer(op_type, dummy) && (current_program->hasUserType(getTypeSpelling(op_type)))) {
								break;
							}
							auto var_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(op)));
							auto type_name = getTypeSpelling(op_type);
							int level_indirection{0};
							auto is_ptr = isCursorTypePointer(op_type, level_indirection);
							auto is_const = isTypeConst(op_type);
							int type_size = is_ptr ? 1 : current_program->getTypeSize(type_name);
							if (is_ptr) {
								//Address new_address = address_generator.generateStackAddress(seg.context_name, type_size);
								auto new_address = address_generator.generateStackAddress(type_size);
								Type type{type_name, is_const, is_ptr, level_indirection, type_size};
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								//auto new_stack_obj = std::make_shared<Object>(new_address, type);
								auto new_stack_obj = std::make_shared<Object>(new_address, type, seg.context_name);
								analyze_functions.at(seg.context_name)->addVariable(new_var);
								//analyze_functions.at(seg.context_name)->addObjToStack(new_stack_obj);
								current_program->addObjToStack(new_stack_obj);
								break;
							} else if (current_program->hasUserType(type_name)) {
								//Address new_address = address_generator.generateStackAddress(seg.context_name, type_size);
								auto new_address = address_generator.generateStackAddress(type_size);
								Type type{type_name, is_const, is_ptr, level_indirection, type_size};
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								//auto new_stack_obj = std::make_shared<Object>(new_address, type);
								auto new_stack_obj = std::make_shared<Object>(new_address, type, seg.context_name);
								for (const auto &field : current_program->getUserTypeByName(type_name)->getFields()) {
									new_stack_obj->addPointerField(field);
								}
								analyze_functions.at(seg.context_name)->addVariable(new_var);
								//analyze_functions.at(seg.context_name)->addObjToStack(new_stack_obj);
								current_program->addObjToStack(new_stack_obj);
								break;
							}
						}
					}
				}
				//printFunctionMemoryState(*analyze_functions.back().at(seg.context_name));
				//printHeapState(heaps.back());
			} else if (kind == CXCursorKind::CXCursor_CXXDeleteExpr) {
				//Обработка оператора delete.
				std::vector<CXCursor> delete_cursors;
				parseCursor(seg.operators.at(i), delete_cursors);
				offset = delete_cursors.size();
				int _offset{0};
				//Получение удаляемого объекта.
				auto delete_data = getSymbolData(delete_cursors, _offset);
				auto delete_obj = getSymbolObject(delete_data, seg.context_name);
				//Удаление его из кучи.
				heap.deleteObjectByAddress(delete_obj->getAddressVal());

				//Режим поиска утечек памяти
				if (find_memory_leak) {
					//Поулчаем все объекты, к которым в данный момент нет доступа
					auto leaked_objects = getLeakedObjects();
					for (auto &f_mem_res : result) {
						//Если среди потерянных объектов есть те, что были потеряны, то отмечаем текущий оператор delete
						//как причину, по которой произошла утечка этиъ объектов. Те объекты, которые в этот момент уже
						//не доступны никак не отмечаются
						auto leak_obj = std::find_if(leaked_objects.cbegin(), leaked_objects.cend(),
							[addr = f_mem_res.heap_obj_address](const std::shared_ptr<Object> &obj) {
								return (obj->getAddress().getAddressValue() == addr.getAddressValue());
							});
						if ((leak_obj != leaked_objects.cend()) && !f_mem_res.leak_op_find) {
							f_mem_res.last_operator = seg.operators.at(i);
							f_mem_res.leak_op_find = true;
						}
					}
				}
				//std::vector<std::shared_ptr<Object> > leaked_objects;
				//hasAccessFromProgram(leaked_objects);
				//if (!leaked_objects.empty()) {
				//	result.push_back(seg.operators.at(i));
				//}
				//printFunctionMemoryState(*analyze_functions.back().at(seg.context_name));
				//printHeapState(heaps.back());
			} else if (kind == CXCursorKind::CXCursor_BinaryOperator) {
				//Парсинг бинарного оператора.

				//Проверка, яв-ся ли он оператором присваивания.
				if (isOperatorAssign(seg.operators.at(i))) {
					std::vector<CXCursor> assign_operators;
					parseCursor(seg.operators.at(i), assign_operators);
					offset = assign_operators.size();

					//Для того, чтобы сохранить информацию о том, какому объекту будет выполнено присваивание.
					SymbolData appropriate_data;
					bool left_value{true};
					/**
					 * В первой части цикла выполняется сбор информации об объекте, которому будет выполнено присваивание.
					 * Во второй части, получение информации о присваиваемом объекте и выполнение приваивания.
					 */
					for (decltype(assign_operators.size()) j = 0; j < assign_operators.size(); ++j) {
						auto op_kind = clang_getCursorKind(assign_operators.at(j));
						auto op_type = clang_getCursorType(assign_operators.at(j));

						//Левая часть оператора присваивния.
						//Если объект яв-ся полем стр-ры.
						if (left_value && (op_kind == CXCursorKind::CXCursor_MemberRefExpr)) {
							if (!isCursorTypePointer(op_type, dummy)) {
								j = assign_operators.size();
								break;
							}
							int _offset{0};
							//Парсинг выражения обращения к полю стр-ры.
							//Смещение индекса цикла.
							appropriate_data = getSymbolData(assign_operators, _offset);
							j += _offset;
							left_value = false;
							continue;
						} else if (left_value && (op_kind == CXCursorKind::CXCursor_DeclRefExpr)) {
							//Если объект яв-ся переменной
							if (!isCursorTypePointer(op_type, dummy)) {
								j = assign_operators.size();
								break;
							}
							int _offset{0};
							//Парсинг и смещение.
							appropriate_data = getSymbolData(assign_operators, _offset);
							j += _offset;
							left_value = false;
							continue;
						}

						//Сохраняем имя переменной, которой присваивается значение
						std::string appropriate_var_name;
						auto appropriate_data_copy = appropriate_data;
						if (find_memory_leak) {
							while (!appropriate_data_copy.fields_name_chain.empty()) {
								appropriate_var_name.insert(0, ".(->)" + appropriate_data_copy.fields_name_chain.top());
								appropriate_data_copy.fields_name_chain.pop();
							}
							appropriate_var_name.insert(0, appropriate_data_copy.var_name);
						}
						//Правая часть оператора присваивания.
						//Если справа оператор new.
						if (op_kind == CXCursorKind::CXCursor_CXXNewExpr) {
							//Получение объекта, которому присваиваем значение.
							auto appropriated_obj = getSymbolObject(appropriate_data, seg.context_name);

							//В режиме поиска утечек памяти, сохраняем исходый адрес переменной, которой присваивается
							//результат оператора new
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}

							//Добавление объекта в кучу и приваивание.
							int type_size = (appropriated_obj->getType().isPointer() && (appropriated_obj->getType().getLevelIndirection() > 1)) ? 1 : current_program->getTypeSize(appropriated_obj->getType().getTypeName());
							auto new_heap_address = address_generator.generateHeapAddress(type_size);
							appropriated_obj->setAddressVal(new_heap_address);
							Type heap_obj_type{appropriated_obj->getType().getTypeName(), appropriated_obj->getType().isConst(), ((appropriated_obj->getType().getLevelIndirection() - 1) > 0), appropriated_obj->getType().getLevelIndirection() - 1, type_size};
							auto heap_object = std::make_shared<Object>(new_heap_address, heap_obj_type, HEAP_CONTEXT);
							if (!heap_object->getType().isPointer() && current_program->hasUserType(heap_object->getType().getTypeName())) {
								for (const auto &field : current_program->getUserTypeByName(heap_object->getType().getTypeName())->getFields()) {
									heap_object->addPointerField(field);
								}
							}

							heap.addObject(heap_object);
							//std::vector<std::shared_ptr<Object> > leaked_objects;
							//hasAccessFromProgram(leaked_objects);
							//if (!leaked_objects.empty()) {
							//	result.push_back(seg.operators.at(i));
							//}
							//Режим поиска утечек памяти
							if (find_memory_leak) {
								auto leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == heap_object->getAddress().getAddressValue()) {
										f_mem_res.allocate_operator = std::make_pair(seg.operators.at(i), appropriate_var_name);
									}
									if (origin_address.getAddressValue() == f_mem_res.heap_obj_address.getAddressValue())
									{
										auto leak_obj = std::find_if(leaked_objects.cbegin(), leaked_objects.cend(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if ((leak_obj != leaked_objects.end()) && !f_mem_res.leak_op_find) {
											f_mem_res.last_operator = seg.operators.at(i);
											f_mem_res.leak_op_find = true;
										}
									}
								}
							}
							break;
						} else if (op_kind == CXCursorKind::CXCursor_CXXNullPtrLiteralExpr) {
							//Если nullptr.

							//Получение объекта, которому вып-ся присваивание и присваивание.
							auto appropriated_obj = getSymbolObject(appropriate_data, seg.context_name);
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}
							appropriated_obj->setAddressVal(Address{0});
							if (find_memory_leak) {
								auto leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == origin_address.getAddressValue()) {
										f_mem_res.operators.push_back({ seg.operators.at(i), "0", appropriate_var_name });
									}
									if (origin_address.getAddressValue() == f_mem_res.heap_obj_address.getAddressValue())
									{
										auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if ((leak_obj != leaked_objects.end()) && !f_mem_res.leak_op_find) {
											f_mem_res.last_operator = seg.operators.at(i);
											f_mem_res.leak_op_find = true;
										}
									}
								}
							}
							break;
						} else if (op_kind == CXCursorKind::CXCursor_DeclRefExpr) {
							//Если справа указатель.

							//Получение присваиваемого объекта.
							auto assigned_var_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(assign_operators.at(j))));
							auto assigned_var = analyze_functions.at(seg.context_name)->getVarByName(assigned_var_name);
							//auto assigned_obj = analyze_functions.at(seg.context_name)->getObjByAddress(assigned_var->getAddress());
							auto assigned_obj = current_program->getObjByAddress(assigned_var->getAddress());

							//Получение объекта, которому вып-ся присваивание и присваивание.
							auto appropriated_obj = getSymbolObject(appropriate_data, seg.context_name);
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());
							
							//std::vector<std::shared_ptr<Object> > leaked_objects;
							//hasAccessFromProgram(leaked_objects);
							//if (!leaked_objects.empty()) {
							//	result.push_back(seg.operators.at(i));
							//}
							if (find_memory_leak) {
								auto leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({seg.operators.at(i), assigned_var->getName(), appropriate_var_name });
									}
									if (f_mem_res.heap_obj_address.getAddressValue() == origin_address.getAddressValue()) {
										auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if ((leak_obj != leaked_objects.end()) && !f_mem_res.leak_op_find) {
											f_mem_res.last_operator = seg.operators.at(i);
											f_mem_res.leak_op_find = true;
										}
									}
								}
							}
							break;
						} else if (op_kind == CXCursorKind::CXCursor_MemberRefExpr) {
							//Если справа поле стр-ры.
							//Парсинг выражения обращения.
							SymbolData assign_data;
							for (decltype(assign_operators.size()) k = j; k < assign_operators.size(); ++k, ++j) {
								auto kind = clang_getCursorKind(assign_operators.at(k));
								auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(assign_operators.at(k))));
								if (kind == CXCursorKind::CXCursor_MemberRefExpr) {
									assign_data.fields_name_chain.push(name);
								} else if (kind == CXCursorKind::CXCursor_DeclRefExpr) {
									assign_data.var_name = name;
									break;
								}
							}

							//Для красивого вывода присваиваемой переменной
							std::string assign_var_name;
							auto assign_data_copy = assign_data;
							if (find_memory_leak) {
								while (!assign_data_copy.fields_name_chain.empty()) {
									assign_var_name.insert(0, ".(->)" + assign_data_copy.fields_name_chain.top());
									assign_data_copy.fields_name_chain.pop();
								}
								assign_var_name.insert(0, assign_data_copy.var_name);
							}

							//Получение присваиваемого объекта.
							auto assigned_obj = getSymbolObject(assign_data, seg.context_name);

							//Получение объекта, которму вып-ся присваивание и присваивание.
							auto appropriated_obj = getSymbolObject(appropriate_data, seg.context_name);
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());
							//std::vector<std::shared_ptr<Object> > leaked_objects;
							//hasAccessFromProgram(leaked_objects);
							//if (!leaked_objects.empty()) {
							//	result.push_back(seg.operators.at(i));
							//}
							if (find_memory_leak) {
								auto leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({seg.operators.at(i), assign_var_name, appropriate_var_name });
									}
									if (f_mem_res.heap_obj_address.getAddressValue() == origin_address.getAddressValue()) {
										auto leak_obj = std::find_if(leaked_objects.cbegin(), leaked_objects.cend(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if ((leak_obj != leaked_objects.cend()) && !f_mem_res.leak_op_find) {
											f_mem_res.last_operator = seg.operators.at(i);
											f_mem_res.leak_op_find = true;
										}
									}
								}
							}
							break;
						}  else if (op_kind == CXCursorKind::CXCursor_CallExpr) {
							//Если справа вызов функции

							//Парсим вызов функции
							std::vector<CXCursor> call_operators;
							parseCursor(assign_operators.at(j), call_operators);

							//Поскольку результат вызова функции будет присвоен последним, то его добавляем в стек первым.
							CallData call_data;
							call_data.evaluate_fun_name = clang_getCString(clang_getCursorSpelling(assign_operators.at(j)));
							call_data.call_expr = assign_operators.at(j);
							call_data.assign_to = AssignTo::Variable;
							call_data.symbol_data = appropriate_data;
							call_data_stack.push(call_data);
							//Если внутри круглых скобок вызова функции стоят вызовы других функций, то запоминаем вызовы этих функций
							//на соотвествующих позициях аргументов, чтобы при возвращении значения из соответсвующей функции, присвоить
							//значение соответсвующему аргументу. Это все проделывается вызовом метода getCallData
							auto call_data_stack_copy = getCallData(assign_operators.at(j), seg.context_name);
							while (!call_data_stack_copy.empty()) {
								call_data_stack.push(call_data_stack_copy.top());
								call_data_stack_copy.pop();
							}
							break;
						}
					}
					//printFunctionMemoryState(*analyze_functions.back().at(seg.context_name));
					//printHeapState(heaps.back());
				}
				
			} else if (kind == CXCursorKind::CXCursor_CallExpr) {
				//Если встретился вызов функции

				//Фиксируем смещение
				std::vector<CXCursor> call_operators;
				parseCursor(seg.operators.at(i), call_operators);
				offset = call_operators.size();

				//Если вызываемая функция из тех, что подлежат рассмотрению в рамках поиска утечек памяти,
				//то запоминаем последовательность функций, в порядке их вызова на позициях аргументнов 
				//вызываемой функции, чтобы присвоить возвращаемое ими значение соответствующим аргументам
				auto call_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(seg.operators.at(i))));
				if (current_program->hasFunction(call_name)) {
					call_data_stack = getCallData(seg.operators.at(i), seg.context_name);
				}
			} else if (kind == CXCursorKind::CXCursor_ReturnStmt) {
				//Если встретился оператор return

				//Фиксируем смещение
				std::vector<CXCursor> return_operators;
				parseCursor(seg.operators.at(i), return_operators);
				offset = return_operators.size();

				for (decltype(return_operators.size()) j = 0; j < return_operators.size(); ++j) {
					auto return_kind = clang_getCursorKind(return_operators.at(j));

					//Если возвращается результат оператора new
					if (return_kind == CXCursorKind::CXCursor_CXXNewExpr) {
						//Размещаем объект в кучу
						auto type_name = getTypeSpelling(clang_getCursorType(return_operators.at(j)));
						int level_indirection{0};
						auto is_ptr = isCursorTypePointer(clang_getCursorType(return_operators.at(j)), level_indirection);
						int size = (is_ptr && (level_indirection > 1)) ? 1 : current_program->hasUserType(type_name)
							? current_program->getUserTypeByName(type_name)->getSize() : 1;
						Type type{type_name, false, (level_indirection > 1), level_indirection - 1, size};

						auto heap_address = address_generator.generateHeapAddress(size);

						auto heap_object = std::make_shared<Object>(heap_address, type, HEAP_CONTEXT);
						if (!heap_object->getType().isPointer() && current_program->hasUserType(heap_object->getType().getTypeName())) {
							for (const auto &field : current_program->getUserTypeByName(heap_object->getType().getTypeName())->getFields()) {
								heap_object->addPointerField(field);
							}
						}
						//Добавляем объект в кучу.
						heap.addObject(heap_object);
						if (find_memory_leak) {
							for (auto &f_mem_res : result) {
								if (f_mem_res.heap_obj_address.getAddressValue() == heap_object->getAddress().getAddressValue()) {
									f_mem_res.allocate_operator = std::make_pair(seg.operators.at(i), "");
									break;
								}
							}
						}
						//Устанавливаем тип возвращаемого значения и сохраняем в него адрес, размещенного в куче объекта
						returned_object->setType(Type(type_name, false, is_ptr, level_indirection, size));
						returned_object->setAddressVal(heap_object->getAddress());
						break;
					} else if (return_kind == CXCursorKind::CXCursor_DeclRefExpr) {
						//Если возвращаемое значение переменная указатель
						std::vector<CXCursor> sym_vector;
						sym_vector.push_back(return_operators.at(j));
						int offset_{0};
						//Получаем возвращаемый объект
						auto symbol_data = getSymbolData(return_operators, offset_);
						auto object = getSymbolObject(symbol_data, seg.context_name);
						//Устанавливаем тип возвращаемого объекта
						returned_object->setType(object->getType());
						//Есди объект не указатель и является пользовательским типом, то в возвращаемый объект добавляются все поля, которые есть
						//и этим полям присваиваются соотвествующие значения
						if (!object->getType().isPointer() && current_program->hasUserType(object->getType().getTypeName())) {
							for (const auto &field : current_program->getUserTypeByName(object->getType().getTypeName())->getFields()) {
								returned_object->addPointerField(field);
								returned_object->getPtrFieldByName(field.getName())->setAddressVal(object->getPtrFieldByName(field.getName())->getAddressVal());
							}
						} else if (object->getType().isPointer()) {
							//Если возвращаемое значение указатель, то в возвращаемый объект помещается адрес, сохраненный в этой переменной
							returned_object->setAddressVal(object->getAddressVal());
						}
						if (find_memory_leak) {
							for (auto &f_mem_res : result) {
								if (f_mem_res.heap_obj_address.getAddressValue() == object->getAddress().getAddressValue()) {
									f_mem_res.operators.push_back({ seg.operators.at(i), symbol_data.var_name, "return"  });
									break;
								}
							}
						}
						break;
					} else if (return_kind == CXCursorKind::CXCursor_MemberRefExpr) {
						//Если возвращаемое значение член структуры

						//Получаем возвращаемый объект
						std::vector<CXCursor> symbol_vec;
						for (decltype(return_operators.size()) k = 0; k < return_operators.size(); ++k) {
							auto sym_kind = clang_getCursorKind(return_operators.at(k));
							symbol_vec.push_back(return_operators.at(k));
							if (sym_kind == CXCursorKind::CXCursor_DeclRefExpr) break;
						}
						int offset_{0};
						auto symbol_data = getSymbolData(symbol_vec, offset_);
						std::string var_name;
						if (find_memory_leak) {
							auto symbol_data_copy = symbol_data;
							while (!symbol_data_copy.fields_name_chain.empty()) {
								var_name.insert(0, ".(->)" + symbol_data_copy.fields_name_chain.top());
								symbol_data_copy.fields_name_chain.pop();
							}
							var_name.insert(0, symbol_data_copy.var_name);
						}
						auto object = getSymbolObject(symbol_data, seg.context_name);
						returned_object->setType(object->getType());
						if (!object->getType().isPointer() && current_program->hasUserType(object->getType().getTypeName())) {
							for (const auto &field : current_program->getUserTypeByName(object->getType().getTypeName())->getFields()) {
								returned_object->addPointerField(field);
								returned_object->getPtrFieldByName(field.getName())->setAddressVal(object->getPtrFieldByName(field.getName())->getAddressVal());
							}
						} else if (object->getType().isPointer()) {
							returned_object->setAddressVal(object->getAddressVal());
						}
						if (find_memory_leak) {
							for (auto &f_mem_res : result) {
								if (f_mem_res.heap_obj_address.getAddressValue() == object->getAddress().getAddressValue()) {
									f_mem_res.allocate_operator = std::make_pair(seg.operators.at(i), symbol_data.var_name);
									break;
								}
							}
						}
						break;
					}
				}

			}
			//std::set<std::shared_ptr<Object> > leaked_objects;
			//leaked_objects = getLeakedObjects();
			//i = (i == 0) ? offset : i + offset;
			i += offset;
		}
	}
}

/*void SymbolEvaluator::evaluatePath_(const std::vector<int> &path, const bool find_memory_leak, std::vector<FindMemoryLeakResult> &result)
{
	//Сбрасываем генератор адресов.
	address_generator.reset();
	//Данный стек контекстов нужен для того, чтобы при выходе из ф-ции, очистить ее от размещенных,
	//в процессе ее выполнения, в ее стеке, переменных и объектов
	std::stack<std::string> context_calls;
	std::stack<CallData> call_data_stack;
	std::string current_context;
	std::shared_ptr<Object> returned_object = std::make_shared<Object>();
	//Для проверки, яв-ся ли некоторый объект указателем.
	int dummy = 0;
	for (const auto &sec_id : path) {

		//Переходы между ф-циями и очистка стека ф-ции, в случае выхода из нее.
		//Если при переходе из одной ф-ции в другую ф-ция завершила свою работу,
		//то чистим стек.
		current_context = path_sections.at(sec_id).context_name;
		if (context_calls.size() > 1) {
			if ((current_context != context_calls.top())
				&& (current_context == (*(&context_calls.top() - 1)))) {
				analyze_functions.at(context_calls.top())->clearStack();
				address_generator.resetStack(context_calls.top());
				context_calls.pop();
			} else if (path_sections.at(sec_id - 1).is_end) {
				if (!call_data_stack.empty()) {
					std::shared_ptr<Object> appropriated_object;
					std::string appropriated_name;
					if (call_data_stack.top().assign_to == AssignTo::Parameter) {
						auto var = analyze_functions.at(call_data_stack.top().fun_name)->getParameterByNum(call_data_stack.top().arg_num).argument;
						auto variable = std::make_shared<Variable>();
						*variable = var;
						variable->setAddress(address_generator.generateStackAddress(call_data_stack.top().fun_name, var.getType().getSize()));
						appropriated_object = std::make_shared<Object>(variable->getAddress(), variable->getType());
						analyze_functions.at(call_data_stack.top().fun_name)->addVariable(variable);
						appropriated_name = variable->getName();
					} else if (call_data_stack.top().assign_to == AssignTo::Variable) {
						SymbolData symbol_data_copy = call_data_stack.top().symbol_data;
						while (!symbol_data_copy.fields_name_chain.empty()) {
							appropriated_name.insert(0, ".(->)" + symbol_data_copy.fields_name_chain.top());
							symbol_data_copy.fields_name_chain.pop();
						}
						appropriated_name.insert(0, symbol_data_copy.var_name);
						appropriated_object = getSymbolObject(call_data_stack.top().symbol_data, current_context);
					}

					if (!returned_object->getType().isPointer() && current_program->hasUserType(returned_object->getType().getTypeName())) {
						for (const auto &field : current_program->getUserTypeByName(returned_object->getType().getTypeName())->getFields()) {
							appropriated_object->addPointerField(field);
							appropriated_object->getPtrFieldByName(field.getName())->setAddressVal(returned_object->getPtrFieldByName(field.getName())->getAddressVal());
						}
					} else if (returned_object->getType().isPointer()) {
						appropriated_object->setAddressVal(returned_object->getAddressVal());
					}
					if (find_memory_leak) {
						std::set<std::shared_ptr<Object> > leaked_objects = getLeakedObjects();
						for (auto &f_mem_res : result) {
							if (f_mem_res.heap_obj_address.getAddressValue() == returned_object->getAddressVal().getAddressValue()) {
								f_mem_res.operators.push_back({ call_data_stack.top().call_expr , call_data_stack.top().evaluate_fun_name, appropriated_name });
							}
							//if (f_mem_res.heap_obj_address.getAddressValue() == origin_address.getAddressValue()) {
							//	auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
							//		[addr = origin_address](const std::shared_ptr<Object> &obj) {
							//			return (obj->getAddress().getAddressValue() == addr.getAddressValue());
							//		});
							//	if (leak_obj != leaked_objects.end()) {
							//		f_mem_res.last_operator = seg.operators.at(i);
							//	}
							//}
						}
					}
					if (call_data_stack.top().assign_to == AssignTo::Parameter) {
						analyze_functions.at(call_data_stack.top().fun_name)->addObjToStack(appropriated_object);
					}
					call_data_stack.pop();
					returned_object = std::make_shared<Object>();
				}
				analyze_functions.at(path_sections.at(sec_id - 1).context_name)->clearStack();
				address_generator.resetStack(path_sections.at(sec_id - 1).context_name);
				context_calls.pop();
				context_calls.push(current_context);
			} else if (current_context != context_calls.top()) {
				context_calls.push(current_context);
			}
		} else if (context_calls.empty() || (context_calls.top() != current_context)) {
			context_calls.push(current_context);
		}

		//Выполнение операторов
		//Обрабатываются следующие операторы:
		//	- оператор объявления(указателя либо стр-ры, среди полей которой могут быть указатели;
		//	- опертор объявления с присваиванием;
		//	- оператор присваивания:
		//		* указателя указателю;
		//		* указателя полю стр-ры
		//		* результат операции new указателю;
		//		* результат операции new полю стр-ры(любой вложенности);
		//		* поля стр-ры(любой вложенности) указателю;
		//		* поля стр-ры(любой вложенности) полю стр-ры(любой вложенности);
		for (size_t i = 0; i < path_sections.at(sec_id).operators.size(); i++) {
			auto kind = clang_getCursorKind(path_sections.at(sec_id).operators.at(i));
			//Смещение, для того, чтобы по завершении обработки оператора, перейти к следующему, с учетом
			//всех, находящихся между двумя операторами, курсоров.
			size_t offset = 0;
			//оператор объявления
			if (kind == CXCursorKind::CXCursor_DeclStmt) {
				std::vector<CXCursor> decl_cursors;
				parseCursor(path_sections.at(sec_id).operators.at(i), decl_cursors);
				offset = decl_cursors.size();
				//проверка на то, вып-ся ли присваивание в объявлении
				if (isOperatorAssign(path_sections.at(sec_id).operators.at(i))) {
					//запоминание имени объвленной переменной, которой будет присвоено значение
					std::string decled_var;
					for (size_t j = 0; j < decl_cursors.size(); j++) {
						auto op_kind = clang_getCursorKind(decl_cursors.at(j));
						auto op_type = clang_getCursorType(decl_cursors.at(j));
						if (op_kind == CXCursorKind::CXCursor_VarDecl) {
							if (!isCursorTypePointer(op_type, dummy)) {
								break;
							}
							//Получаем об объявляемой переменной необходимую информацию:
							//	- имя
							//	- тип
							//	- указатель или нет (с учетом уровня косвенности)
							//	- const или нет
							//	- размер типа
							std::string var_name = clang_getCString(clang_getCursorSpelling(decl_cursors.at(j)));
							std::string type_name = getTypeSpelling(op_type);
							int level_indirection = 0;
							bool is_ptr = isCursorTypePointer(op_type, level_indirection);
							bool is_const = isTypeConst(op_type);
							int type_size = is_ptr ? 1 : current_program->getTypeSize(type_name);
							if (is_ptr) {
								decled_var = var_name;
								//Генерируем адрес в стеке и создаем переменную и объект в стеке, добавляя их в программу
								Address new_address = address_generator.generateStackAddress(path_sections.at(sec_id).context_name, type_size);
								Type type(type_name, is_const, is_ptr, level_indirection, type_size);
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								auto new_stack_obj = std::make_shared<Object>(new_address, type);
								analyze_functions.at(path_sections.at(sec_id).context_name)->addVariable(new_var);
								analyze_functions.at(path_sections.at(sec_id).context_name)->addObjToStack(new_stack_obj);
							} else if (current_program->hasUserType(type_name)) {
								decled_var = var_name;
								Address new_address = address_generator.generateStackAddress(path_sections.at(sec_id).context_name, type_size);
								Type type(type_name, is_const, is_ptr, level_indirection, type_size);
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								auto new_stack_obj = std::make_shared<Object>(new_address, type);
								//Если переменная яв-ся объектом пользовательского типа, то добавляем в него необходимые поля,
								//в соответствии описанием пользовательского типа.
								for (const auto &field : current_program->getUserTypeByName(type_name)->getFields()) {
									new_stack_obj->addPointerField(field);
								}
								analyze_functions.at(path_sections.at(sec_id).context_name)->addVariable(new_var);
								analyze_functions.at(path_sections.at(sec_id).context_name)->addObjToStack(new_stack_obj);
							}
						} else if (op_kind == CXCursorKind::CXCursor_DeclRefExpr) {
							if (!isCursorTypePointer(op_type, dummy)) {
								break;
							}
							std::string assigned_var_name = clang_getCString(clang_getCursorSpelling(decl_cursors.at(j)));
							//Получаем приcваиваемый объект.
							auto assigned_var = analyze_functions.at(path_sections.at(sec_id).context_name)->getVarByName(assigned_var_name);
							auto assigned_obj = analyze_functions.at(path_sections.at(sec_id).context_name)->getObjByAddress(assigned_var->getAddress());
							//Получаем объект, которому будет выполняться присваивание.
							auto appropriated_var = analyze_functions.at(path_sections.at(sec_id).context_name)->getVarByName(decled_var);
							auto appropriated_obj = analyze_functions.at(path_sections.at(sec_id).context_name)->getObjByAddress(appropriated_var->getAddress());
							//Т.к. рассматривается присваивание только указателей, то помещаем адрес, который хранится в присваиваемом объекте,
							//объекту которому вып-ся присваивание.
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());
							if (find_memory_leak) {
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({ path_sections.at(sec_id).operators.at(i), assigned_var->getName(), appropriated_var->getName() });
										break;
									}
								}
							}
						} else if (op_kind == CXCursorKind::CXCursor_CXXNewExpr) {
							//Получаем объект, которому будет выполняться присваивание.
							auto appropriated_var = analyze_functions.at(path_sections.at(sec_id).context_name)->getVarByName(decled_var);
							auto appropriated_obj = analyze_functions.at(path_sections.at(sec_id).context_name)->getObjByAddress(appropriated_var->getAddress());
							//Получаем размер объекта, который будет размещен в куче. Для этого проверяем уровень косвенности указателя, если он равен 1, то в кучу
							//будет размещен объект, если больше 1, значит в кучу будет размещен указатель.
							int type_size = (appropriated_obj->getType().isPointer() && (appropriated_obj->getType().getLevelIndirection() > 1)) ? 1 : current_program->getTypeSize(appropriated_var->getType().getTypeName());
							//Генерируем адрес в куче.
							Address new_heap_address = address_generator.generateHeapAddress(type_size);
							//Присваиваем его.
							appropriated_obj->setAddressVal(new_heap_address);
							//Создаем объект в куче, в соответствии с параметрами объекта указателя и добавляем поля, если это объект пользовательского типа.
							Type heap_obj_type(appropriated_obj->getType().getTypeName(), appropriated_obj->getType().isConst(), ((appropriated_obj->getType().getLevelIndirection() - 1) > 0), appropriated_obj->getType().getLevelIndirection() - 1, type_size);
							auto heap_object = std::make_shared<Object>(new_heap_address, heap_obj_type);
							if (!heap_object->getType().isPointer() && current_program->hasUserType(heap_object->getType().getTypeName())) {
								for (const auto &field : current_program->getUserTypeByName(heap_object->getType().getTypeName())->getFields()) {
									heap_object->addPointerField(field);
								}
							}
							//Добавляем объект в кучу.
							heap.addObject(heap_object);
							if (find_memory_leak) {
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == heap_object->getAddress().getAddressValue()) {
										f_mem_res.allocate_operator = std::make_pair(path_sections.at(sec_id).operators.at(i), appropriated_var->getName());
										break;
									}
								}
							}
						} else if (op_kind == CXCursorKind::CXCursor_MemberRefExpr) {
							if (!isCursorTypePointer(op_type, dummy)) {
								break;
							}
							//Парсинг выражения обращения к полю стр-ры.
							SymbolData assign_data;
							std::string assigned_var_name;
							int field_access_count = 0;
							for (size_t k = j; k < decl_cursors.size(); k++, j++, field_access_count++) {
								auto kind = clang_getCursorKind(decl_cursors.at(k));
								std::string name = clang_getCString(clang_getCursorSpelling(decl_cursors.at(k)));
								if (kind == CXCursorKind::CXCursor_MemberRefExpr) {
									assign_data.fields_name_chain.push(name);
									if (find_memory_leak) {
										//Для красивого выведения на экран присваивания полю стр-ры.
										assigned_var_name.insert(0, ".(->)" + name);
										field_access_count = 0;
									}
								} else if (kind == CXCursorKind::CXCursor_DeclRefExpr) {
									assign_data.var_name = name;
									if (find_memory_leak) {
										//Для красивого выведения на экран присваивания полю стр-ры.
										assigned_var_name.insert(0, name);
										field_access_count = 0;
									}
									break;
								}
							}

							//Получение присваиваемого объекта.
							auto assigned_obj = getSymbolObject(assign_data, path_sections.at(sec_id).context_name);

							//Получение итогового объкта, которму будет выполнено присваивание.
							auto appropriated_var = analyze_functions.at(path_sections.at(sec_id).context_name)->getVarByName(decled_var);
							auto appropriated_obj = analyze_functions.at(path_sections.at(sec_id).context_name)->getObjByAddress(appropriated_var->getAddress());
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());

							if (find_memory_leak) {
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({ path_sections.at(sec_id).operators.at(i), assigned_var_name, appropriated_var->getName() });
										break;
									}
								}
							}
						}
					}
				} else {
					//Обычное объявление, без присваивания. В рез-те будут размещены переменная и объект в стек ф-ции.
					for (const auto &op : decl_cursors) {
						auto op_kind = clang_getCursorKind(op);
						auto op_type = clang_getCursorType(op);
						if (op_kind == CXCursorKind::CXCursor_VarDecl) {
							if (!isCursorTypePointer(op_type, dummy) && (current_program->hasUserType(getTypeSpelling(op_type)))) {
								break;
							}
							std::string var_name = clang_getCString(clang_getCursorSpelling(op));
							std::string type_name = getTypeSpelling(op_type);
							int level_indirection = 0;
							bool is_ptr = isCursorTypePointer(op_type, level_indirection);
							bool is_const = isTypeConst(op_type);
							int type_size = is_ptr ? 1 : current_program->getTypeSize(type_name);
							if (is_ptr) {
								Address new_address = address_generator.generateStackAddress(path_sections.at(sec_id).context_name, type_size);
								Type type(type_name, is_const, is_ptr, level_indirection, type_size);
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								auto new_stack_obj = std::make_shared<Object>(new_address, type);
								analyze_functions.at(path_sections.at(sec_id).context_name)->addVariable(new_var);
								analyze_functions.at(path_sections.at(sec_id).context_name)->addObjToStack(new_stack_obj);
								break;
							} else if (current_program->hasUserType(type_name)) {
								Address new_address = address_generator.generateStackAddress(path_sections.at(sec_id).context_name, type_size);
								Type type(type_name, is_const, is_ptr, level_indirection, type_size);
								auto new_var = std::make_shared<Variable>(var_name, type, new_address);
								auto new_stack_obj = std::make_shared<Object>(new_address, type);
								for (const auto &field : current_program->getUserTypeByName(type_name)->getFields()) {
									new_stack_obj->addPointerField(field);
								}
								analyze_functions.at(path_sections.at(sec_id).context_name)->addVariable(new_var);
								analyze_functions.at(path_sections.at(sec_id).context_name)->addObjToStack(new_stack_obj);
								break;
							}
						}
					}
				}
			} else if (kind == CXCursorKind::CXCursor_CXXDeleteExpr) {
				//Обработка оператора delete.
				std::vector<CXCursor> delete_cursors;
				parseCursor(path_sections.at(sec_id).operators.at(i), delete_cursors);
				offset = delete_cursors.size();
				int _offset = 0;
				//Получение удаляемого объекта.
				SymbolData delete_data = getSymbolData(delete_cursors, _offset);
				auto delete_obj = getSymbolObject(delete_data, path_sections.at(sec_id).context_name);
				//Удаление его из кучи.
				heap.deleteObjectByAddress(delete_obj->getAddressVal());
				if (find_memory_leak) {
					std::set <std::shared_ptr<Object> > leaked_objects = getLeakedObjects();
					for (auto &f_mem_res : result) {
						auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
							[addr = f_mem_res.heap_obj_address](const std::shared_ptr<Object> &obj) {
							return (obj->getAddress().getAddressValue() == addr.getAddressValue());
						});
						if (leak_obj != leaked_objects.end()) {
							f_mem_res.last_operator = path_sections.at(sec_id).operators.at(i);
						}
					}
				}
				//std::vector<std::shared_ptr<Object> > leaked_objects;
				//hasAccessFromProgram(leaked_objects);
				//if (!leaked_objects.empty()) {
				//	result.push_back(seg.operators.at(i));
				//}
			} else if (kind == CXCursorKind::CXCursor_BinaryOperator) {
				//Парсинг бинарного оператора.

				//Проверка, яв-ся ли он оператором присваивания.
				if (isOperatorAssign(path_sections.at(sec_id).operators.at(i))) {
					std::vector<CXCursor> assign_operators;
					parseCursor(path_sections.at(sec_id).operators.at(i), assign_operators);
					offset = assign_operators.size();

					//Для того, чтобы сохранить информацию о том, какому объекту будет выполнено присваивание.
					SymbolData appropriate_data;
					bool left_value = true;

					//В первой части цикла выполняется сбор информации об объекте, которому будет выполнено присваивание.
					//Во второй части, получение информации о присваиваемом объекте и выполнение приваивания. 
					for (size_t j = 0; j < assign_operators.size(); j++) {
						auto op_kind = clang_getCursorKind(assign_operators.at(j));
						auto op_type = clang_getCursorType(assign_operators.at(j));

						//Левая часть оператора присваивния.
						//Если объект яв-ся полем стр-ры.
						if (left_value && (op_kind == CXCursorKind::CXCursor_MemberRefExpr)) {
							if (!isCursorTypePointer(op_type, dummy)) {
								j = assign_operators.size();
								break;
							}
							int _offset = 0;
							//Парсинг выражения обращения к полю стр-ры.
							//Смещение индекса цикла.
							appropriate_data = getSymbolData(assign_operators, _offset);
							j += _offset;
							left_value = false;
							continue;
						} else if (left_value && (op_kind == CXCursorKind::CXCursor_DeclRefExpr)) {
							//Если объект яв-ся переменной
							if (!isCursorTypePointer(op_type, dummy)) {
								j = assign_operators.size();
								break;
							}
							int _offset = 0;
							//Парсинг и смещение.
							appropriate_data = getSymbolData(assign_operators, _offset);
							j += _offset;
							left_value = false;
							continue;
						}

						//Правая часть оператора присваивания.
						std::string appropriate_var_name;
						SymbolData appropriate_data_copy = appropriate_data;
						if (find_memory_leak) {
							while (!appropriate_data_copy.fields_name_chain.empty()) {
								appropriate_var_name.insert(0, ".(->)" + appropriate_data_copy.fields_name_chain.top());
								appropriate_data_copy.fields_name_chain.pop();
							}
							appropriate_var_name.insert(0, appropriate_data_copy.var_name);
						}
						//Если справа оператор new.
						if (op_kind == CXCursorKind::CXCursor_CXXNewExpr) {
							//Получение объекта, которому присваиваем значение.
							auto appropriated_obj = getSymbolObject(appropriate_data, path_sections.at(sec_id).context_name);
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}

							//Добавление объекта в кучу и приваивание.
							int type_size = (appropriated_obj->getType().isPointer() && (appropriated_obj->getType().getLevelIndirection() > 1)) ? 1 : current_program->getTypeSize(appropriated_obj->getType().getTypeName());
							Address new_heap_address = address_generator.generateHeapAddress(type_size);
							appropriated_obj->setAddressVal(new_heap_address);
							Type heap_obj_type(appropriated_obj->getType().getTypeName(), appropriated_obj->getType().isConst(), ((appropriated_obj->getType().getLevelIndirection() - 1) > 0), appropriated_obj->getType().getLevelIndirection() - 1, type_size);
							auto heap_object = std::make_shared<Object>(new_heap_address, heap_obj_type);
							if (!heap_object->getType().isPointer() && current_program->hasUserType(heap_object->getType().getTypeName())) {
								for (const auto &field : current_program->getUserTypeByName(heap_object->getType().getTypeName())->getFields()) {
									heap_object->addPointerField(field);
								}
							}

							heap.addObject(heap_object);
							//std::vector<std::shared_ptr<Object> > leaked_objects;
							//hasAccessFromProgram(leaked_objects);
							//if (!leaked_objects.empty()) {
							//	result.push_back(seg.operators.at(i));
							//}
							if (find_memory_leak) {
								std::set<std::shared_ptr<Object> > leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == heap_object->getAddress().getAddressValue()) {
										f_mem_res.allocate_operator = std::make_pair(path_sections.at(sec_id).operators.at(i), appropriate_var_name);
									}
									if (origin_address.getAddressValue() == f_mem_res.heap_obj_address.getAddressValue())
									{
										auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if (leak_obj != leaked_objects.end()) {
											f_mem_res.last_operator = path_sections.at(sec_id).operators.at(i);
										}
									}
								}
							}
							break;
						} else if (op_kind == CXCursorKind::CXCursor_DeclRefExpr) {
							//Если справа указатель.

							//Получение присваиваемого объекта.
							std::string assigned_var_name = clang_getCString(clang_getCursorSpelling(assign_operators.at(j)));
							auto assigned_var = analyze_functions.at(path_sections.at(sec_id).context_name)->getVarByName(assigned_var_name);
							auto assigned_obj = analyze_functions.at(path_sections.at(sec_id).context_name)->getObjByAddress(assigned_var->getAddress());

							//Получение объекта, которому вып-ся присваивание и присваивание.
							auto appropriated_obj = getSymbolObject(appropriate_data, path_sections.at(sec_id).context_name);
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());

							//std::vector<std::shared_ptr<Object> > leaked_objects;
							//hasAccessFromProgram(leaked_objects);
							//if (!leaked_objects.empty()) {
							//	result.push_back(seg.operators.at(i));
							//}
							if (find_memory_leak) {
								std::set<std::shared_ptr<Object> > leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({ path_sections.at(sec_id).operators.at(i), assigned_var->getName(), appropriate_var_name });
									}
									if (f_mem_res.heap_obj_address.getAddressValue() == origin_address.getAddressValue()) {
										auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if (leak_obj != leaked_objects.end()) {
											f_mem_res.last_operator = path_sections.at(sec_id).operators.at(i);
										}
									}
								}
							}
							break;
						} else if (op_kind == CXCursorKind::CXCursor_MemberRefExpr) {
							//Если справа поле стр-ры.
							//Парсинг выражения обращения.
							SymbolData assign_data;
							for (size_t k = j; k < assign_operators.size(); k++, j++) {
								auto kind = clang_getCursorKind(assign_operators.at(k));
								std::string name = clang_getCString(clang_getCursorSpelling(assign_operators.at(k)));
								if (kind == CXCursorKind::CXCursor_MemberRefExpr) {
									assign_data.fields_name_chain.push(name);
								} else if (kind == CXCursorKind::CXCursor_DeclRefExpr) {
									assign_data.var_name = name;
									break;
								}
							}

							std::string assign_var_name;
							SymbolData assign_data_copy = assign_data;
							if (find_memory_leak) {
								while (!assign_data_copy.fields_name_chain.empty()) {
									assign_var_name.insert(0, ".(->)" + assign_data_copy.fields_name_chain.top());
									assign_data_copy.fields_name_chain.pop();
								}
								assign_var_name.insert(0, assign_data_copy.var_name);
							}

							//Получение присваиваемого объекта.
							auto assigned_obj = getSymbolObject(assign_data, path_sections.at(sec_id).context_name);

							//Получение объекта, которму вып-ся присваивание и присваивание.
							auto appropriated_obj = getSymbolObject(appropriate_data, path_sections.at(sec_id).context_name);
							Address origin_address;
							if (find_memory_leak) {
								origin_address = appropriated_obj->getAddressVal();
							}
							appropriated_obj->setAddressVal(assigned_obj->getAddressVal());
							//std::vector<std::shared_ptr<Object> > leaked_objects;
							//hasAccessFromProgram(leaked_objects);
							//if (!leaked_objects.empty()) {
							//	result.push_back(seg.operators.at(i));
							//}
							if (find_memory_leak) {
								std::set<std::shared_ptr<Object> > leaked_objects = getLeakedObjects();
								for (auto &f_mem_res : result) {
									if (f_mem_res.heap_obj_address.getAddressValue() == assigned_obj->getAddressVal().getAddressValue()) {
										f_mem_res.operators.push_back({ path_sections.at(sec_id).operators.at(i), assign_var_name, appropriate_var_name });
									}
									if (f_mem_res.heap_obj_address.getAddressValue() == origin_address.getAddressValue()) {
										auto leak_obj = std::find_if(leaked_objects.begin(), leaked_objects.end(),
											[addr = origin_address](const std::shared_ptr<Object> &obj) {
												return (obj->getAddress().getAddressValue() == addr.getAddressValue());
											});
										if (leak_obj != leaked_objects.end()) {
											f_mem_res.last_operator = path_sections.at(sec_id).operators.at(i);
										}
									}
								}
							}
							break;
						} else if (op_kind == CXCursorKind::CXCursor_CallExpr) {
							std::vector<CXCursor> call_operators;
							parseCursor(assign_operators.at(j), call_operators);

							call_data_stack = getCallData(assign_operators.at(j), path_sections.at(sec_id).context_name);
							CallData call_data;
							call_data.evaluate_fun_name = clang_getCString(clang_getCursorSpelling(assign_operators.at(j)));
							call_data.call_expr = path_sections.at(sec_id).operators.at(i);
							call_data.assign_to = AssignTo::Variable;
							call_data.symbol_data = appropriate_data;
							call_data_stack.push(call_data);
							break;
						}
					}
				}
			} else if (kind == CXCursorKind::CXCursor_CallExpr) {
				std::vector<CXCursor> call_operators;
				parseCursor(path_sections.at(sec_id).operators.at(i), call_operators);
				offset = call_operators.size();

				std::string call_name = clang_getCString(clang_getCursorSpelling(path_sections.at(sec_id).operators.at(i)));
				if (current_program->hasFunction(call_name)) {
					call_data_stack = getCallData(path_sections.at(sec_id).operators.at(i), path_sections.at(sec_id).context_name);
				}
			} else if (kind == CXCursorKind::CXCursor_ReturnStmt) {
				std::vector<CXCursor> return_operators;
				parseCursor(path_sections.at(sec_id).operators.at(i), return_operators);
				offset = return_operators.size();

				for (size_t j = 0; j < return_operators.size(); j++) {
					auto return_kind = clang_getCursorKind(return_operators.at(j));

					if (return_kind == CXCursorKind::CXCursor_CXXNewExpr) {
						std::string type_name = getTypeSpelling(clang_getCursorType(return_operators.at(j)));
						int level_indirection = 0;
						bool is_ptr = isCursorTypePointer(clang_getCursorType(return_operators.at(j)), level_indirection);
						int size = (is_ptr && (level_indirection > 1)) ? 1 : current_program->hasUserType(type_name)
							? current_program->getUserTypeByName(type_name)->getSize() : 1;
						Type type(type_name, false, (level_indirection > 1), level_indirection - 1, size);

						Address heap_address = address_generator.generateHeapAddress(size);

						auto heap_object = std::make_shared<Object>(heap_address, type);
						if (!heap_object->getType().isPointer() && current_program->hasUserType(heap_object->getType().getTypeName())) {
							for (const auto &field : current_program->getUserTypeByName(heap_object->getType().getTypeName())->getFields()) {
								heap_object->addPointerField(field);
							}
						}
						//Добавляем объект в кучу.
						heap.addObject(heap_object);
						if (find_memory_leak) {
							for (auto &f_mem_res : result) {
								if (f_mem_res.heap_obj_address.getAddressValue() == heap_object->getAddress().getAddressValue()) {
									f_mem_res.allocate_operator = std::make_pair(path_sections.at(sec_id).operators.at(i), "");
									break;
								}
							}
						}
						returned_object->setType(Type(type_name, false, is_ptr, level_indirection, size));
						returned_object->setAddressVal(heap_object->getAddress());
						break;
					} else if (return_kind == CXCursorKind::CXCursor_DeclRefExpr) {
						std::vector<CXCursor> sym_vector;
						sym_vector.push_back(return_operators.at(j));
						int offset_ = 0;
						SymbolData symbol_data = getSymbolData(return_operators, offset_);
						auto object = getSymbolObject(symbol_data, path_sections.at(sec_id).context_name);
						returned_object->setType(object->getType());
						if (!object->getType().isPointer() && current_program->getUserTypeByName(object->getType().getTypeName())) {
							for (const auto &field : current_program->getUserTypeByName(object->getType().getTypeName())->getFields()) {
								returned_object->addPointerField(field);
								returned_object->getPtrFieldByName(field.getName())->setAddressVal(object->getPtrFieldByName(field.getName())->getAddressVal());
							}
						} else if (object->getType().isPointer()) {
							returned_object->setAddressVal(object->getAddressVal());
						}
						if (find_memory_leak) {
							for (auto &f_mem_res : result) {
								if (f_mem_res.heap_obj_address.getAddressValue() == object->getAddress().getAddressValue()) {
									f_mem_res.allocate_operator = std::make_pair(path_sections.at(sec_id).operators.at(i), symbol_data.var_name);
									break;
								}
							}
						}
						break;
					} else if (return_kind == CXCursorKind::CXCursor_MemberRefExpr) {
						std::vector<CXCursor> symbol_vec;
						for (size_t k = 0; k < return_operators.size(); k++) {
							auto sym_kind = clang_getCursorKind(return_operators.at(k));
							symbol_vec.push_back(return_operators.at(k));
							if (sym_kind == CXCursorKind::CXCursor_DeclRefExpr) break;
						}
						int offset_ = 0;
						SymbolData symbol_data = getSymbolData(symbol_vec, offset_);
						std::string var_name;
						if (find_memory_leak) {
							SymbolData symbol_data_copy = symbol_data;
							while (!symbol_data_copy.fields_name_chain.empty()) {
								var_name.insert(0, ".(->)" + symbol_data_copy.fields_name_chain.top());
								symbol_data_copy.fields_name_chain.pop();
							}
							var_name.insert(0, symbol_data_copy.var_name);
						}
						auto object = getSymbolObject(symbol_data, path_sections.at(sec_id).context_name);
						returned_object->setType(object->getType());
						if (!object->getType().isPointer() && current_program->getUserTypeByName(object->getType().getTypeName())) {
							for (const auto &field : current_program->getUserTypeByName(object->getType().getTypeName())->getFields()) {
								returned_object->addPointerField(field);
								returned_object->getPtrFieldByName(field.getName())->setAddressVal(object->getPtrFieldByName(field.getName())->getAddressVal());
							}
						} else if (object->getType().isPointer()) {
							returned_object->setAddressVal(object->getAddressVal());
						}
						if (find_memory_leak) {
							for (auto &f_mem_res : result) {
								if (f_mem_res.heap_obj_address.getAddressValue() == object->getAddress().getAddressValue()) {
									f_mem_res.allocate_operator = std::make_pair(path_sections.at(sec_id).operators.at(i), symbol_data.var_name);
									break;
								}
							}
						}
						break;
					}
				}

			}
			//std::set<std::shared_ptr<Object> > leaked_objects;
			//leaked_objects = getLeakedObjects();
			//i = (i == 0) ? offset : i + offset;
			i += offset;
		}
	}

}*/

static bool alreadyChosen(const std::shared_ptr<Object> obj, const std::shared_ptr<HeapObjectConnections> obj_connections)
{
	bool chosen{false};
	auto current_obj_connections = obj_connections;
	while (current_obj_connections) {
		if (current_obj_connections->current_object) break;
		if (obj->getAddress().getAddressValue() == current_obj_connections->current_object->getAddress().getAddressValue()) {
			chosen = true;
			break;
		}
		for (const auto &linked_obj : current_obj_connections->linked_objects) {
			if (obj->getAddress().getAddressValue() == linked_obj->getAddress().getAddressValue()) {
				chosen = true;
				break;
			}
		}
		if (chosen) break;
		current_obj_connections = current_obj_connections->object_link;
	}

	return chosen;

}

std::set<std::shared_ptr<Object> > SymbolEvaluator::getLeakedObjects()
{
	std::set<std::shared_ptr<Object> > leaked_objects;
	std::set<std::shared_ptr<Object> > has_access;
	for (const auto &heap_obj : heap.getHeap()) {
		if ((leaked_objects.count(heap_obj) > 0)
			|| (has_access.count(heap_obj) > 0)) {
			continue;
		}
		auto heap_object_connections = std::make_shared<HeapObjectConnections>();
		heap_object_connections->current_object = heap_obj;
		std::set<std::shared_ptr<Object> > leaked_objects_;
		std::set<std::shared_ptr<Object> > has_access_;
		auto has = hasAccessFromProgram(heap_object_connections, leaked_objects_, has_access_);
		leaked_objects.insert(leaked_objects_.begin(), leaked_objects_.end());
		has_access.insert(has_access_.begin(), has_access_.end());
		if (has) {
			has_access.insert(heap_object_connections->current_object);
		} else {
			leaked_objects.insert(heap_object_connections->current_object);
		}
	}

	return leaked_objects;
}

bool SymbolEvaluator::hasAccessFromProgram(const std::shared_ptr<HeapObjectConnections> object,
	std::set<std::shared_ptr<Object> > &leaked_objects,
	std::set<std::shared_ptr<Object> > &has_access)
{
	//for (const auto &fun : analyze_functions) {
		//for (const auto &stack_obj : fun.second->getStack()) {
		for (const auto &stack_obj : current_program->getStack()) {
			if (stack_obj->getType().isPointer()
				&& (stack_obj->getAddressVal().getAddressValue() == object->current_object->getAddress().getAddressValue())) {
				return true;
			} else if (stack_obj->getType().isPointer() && current_program->hasUserType(stack_obj->getType().getTypeName())) {
				auto heap_obj = heap.getObjectByAddress(stack_obj->getAddressVal());
				if (!heap_obj) {
					continue;
				}
				for (const auto &field : heap_obj->getPtrFields()) {
					if (field.second->getAddressVal().getAddressValue() == object->current_object->getAddress().getAddressValue()) {
						return true;
					}
				}
			} else if (current_program->hasUserType(stack_obj->getType().getTypeName())) {
				for (const auto &field : stack_obj->getPtrFields()) {
					if (field.second->getAddressVal().getAddressValue() == object->current_object->getAddress().getAddressValue()) {
						return true;
					}
				}
			}
		}
	//}

	for (const auto &heap_obj : heap.getHeap()) {
		if (heap_obj->getType().isPointer()
			&& (heap_obj->getAddressVal().getAddressValue() == object->current_object->getAddress().getAddressValue())
			&& (!alreadyChosen(heap_obj, object)))
		{
			object->linked_objects.push_back(heap_obj);
		}  else if (current_program->hasUserType(heap_obj->getType().getTypeName())) {
			for (const auto &field : heap_obj->getPtrFields()) {
				if ((field.second->getAddressVal().getAddressValue() == object->current_object->getAddress().getAddressValue())
					&& (!alreadyChosen(heap_obj, object)))
				{
					object->linked_objects.push_back(heap_obj);
				}
			}
		}
	}

	bool res{false};
	for (const auto &linked_object : object->linked_objects) {
		auto heap_object_connections = std::make_shared<HeapObjectConnections>();
		heap_object_connections->current_object = linked_object;
		heap_object_connections->object_link = object;
		std::set<std::shared_ptr<Object> > leaked_objects_;
		std::set<std::shared_ptr<Object> > has_access_;
		res = hasAccessFromProgram(heap_object_connections, leaked_objects_, has_access_);
		leaked_objects.insert(leaked_objects_.begin(), leaked_objects_.end());
		has_access.insert(has_access_.begin(), has_access_.end());
		if (res) {
			has_access.insert(heap_object_connections->current_object);
			return true;
		} else {
			leaked_objects.insert(heap_object_connections->current_object);
		}
	}

	return res;
}

/*
 * Метод возвращающий стек структур. Этот стек представляет собой последовательность вызываемых функций
 * и, если вызовы этих функций стоят на местах передаваемых в функцию аргументов, то в структурах указывается
 * возвращаемое значение какой функции, какому аргументу вызываемой функции необходимо присвоить.
 * Например function(fun_1(), arg, fun_2()). В результате выполнения этой функции будет возвращен стек, в котором
 * будем две структуры, в следующем порядке и со следующими полями:
	- на вершине стека будет структура, у которой имя вызываемой ф-ции будет - function, имя выаолняемой ф-ции - fun_2(),
	  arg_num - 2, assign_to - AssignTo::Parameter(это поле нужно для парсинга на этапе получения возвращаемого значения),
	  call_expr - fun_2_cursor для указания в режиме поиска утечек памяти имени функции,  если из нее вернется потерянный элемент,
	  symbol_data - {} поле, в котором сохранаяется имя переменной, в случае, если на месте аргумента стоит не вызов функции,
	  а имя переменной, значение которой сохраняется
	- следующим элементом стека будет структура, у которой имя вызываемой ф-ции будет - function, имя выполняемой ф-ции - "",
	  arg_num - 1, assign_to - AssignTo::Parameter, call_expr - CXCursorNull, symbol_data - arg
	- последним элементом стека будет структура, у которой fun_name - function, evaluate_fun_name - fun_1, arg_num - 0,
	  assign_to - AssignTo::Parameter, call_expr - fun_1_cursor, symbol_data = {}
*/
std::stack<CallData> SymbolEvaluator::getCallData(const CXCursor &call_stmt, const std::string &context_name)
{
	std::stack<CallData> call_data_stack;
	auto fun_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(call_stmt)));
	std::vector<CXCursor> call_cursors;
	parseCursor(call_stmt, call_cursors);
	call_cursors.erase(call_cursors.begin());
	call_cursors.erase(call_cursors.begin());
	int current_arg_num{0};
	for (decltype(call_cursors.size()) i = 0; i < call_cursors.size(); ++i) {
		auto kind = clang_getCursorKind(call_cursors.at(i));
		if (kind == CXCursorKind::CXCursor_CallExpr) {
			CallData call_data;
			call_data.call_expr = call_stmt;
			call_data.arg_num = current_arg_num;
			call_data.fun_name = fun_name;
			call_data.assign_to = AssignTo::Parameter;
			call_data.evaluate_fun_name = clang_getCString(clang_getCursorSpelling(call_cursors.at(i)));
			std::vector<CXCursor> for_offset;
			parseCursor(call_cursors.at(i), for_offset);
			auto offset = for_offset.size();
			auto call_data_stack_ = getCallData(call_cursors.at(i), context_name);
			call_data_stack_.push(call_data);
			while (!call_data_stack_.empty()) {
				call_data_stack.push(call_data_stack_.top());
				call_data_stack_.pop();
			}
			++current_arg_num;
			i += offset;
		} else if ((kind == CXCursorKind::CXCursor_MemberRefExpr)
			|| (kind == CXCursorKind::CXCursor_DeclRefExpr))
		{
			std::vector<CXCursor> symbol_vec;
			size_t offset{0};
			for (decltype(call_cursors.size()) j = i; j < call_cursors.size(); ++j, ++offset) {
				auto sym_kind = clang_getCursorKind(call_cursors.at(j));
				symbol_vec.push_back(call_cursors.at(j));
				if (sym_kind == CXCursorKind::CXCursor_DeclRefExpr) break;
			}
			if (analyze_functions.at(fun_name)->getParameterByNum(current_arg_num).is_used) {
				int offset_{0};
				auto symbol_data = getSymbolData(symbol_vec, offset_);
				auto var = analyze_functions.at(fun_name)->getParameterByNum(current_arg_num).argument;
				auto variable = std::make_shared<Variable>();
				*variable = var;
				//variable->setAddress(address_generator.generateStackAddress(fun_name, var.getType().getSize()));
				variable->setAddress(address_generator.generateStackAddress(var.getType().getSize()));
				auto object = getSymbolObject(symbol_data, context_name);
				//auto added_object = std::make_shared<Object>(variable->getAddress(), variable->getType());
				auto added_object = std::make_shared<Object>(variable->getAddress(), variable->getType(), fun_name);
				analyze_functions.at(fun_name)->addVariable(variable);
				if (!object->getType().isPointer() && current_program->hasUserType(object->getType().getTypeName())) {
					for (const auto &field : current_program->getUserTypeByName(object->getType().getTypeName())->getFields()) {
						added_object->addPointerField(field);
						added_object->getPtrFieldByName(field.getName())->setAddressVal(object->getPtrFieldByName(field.getName())->getAddressVal());
					}
				} else if (object->getType().isPointer()) {
					added_object->setAddressVal(object->getAddressVal());
				}
				//analyze_functions.at(fun_name)->addObjToStack(added_object);
				current_program->addObjToStack(added_object);
			}
			i += offset;
		}
	}

	return call_data_stack;
}

/**
 * Парсинг выражения, которому выполняется присваивание или которое будет присвоено.
 */
SymbolData SymbolEvaluator::getSymbolData(const std::vector<CXCursor> &op_cursors, int &offset)
{
	SymbolData symbol_data;
	for (decltype(op_cursors.size()) i = 0; i < op_cursors.size(); ++i, ++offset) {
		auto kind = clang_getCursorKind(op_cursors.at(i));
		auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(op_cursors.at(i))));
		//Если объект яв-ся полем стр-ры, то добавляем его имя в цепочку обращений к полям
		if (kind == CXCursorKind::CXCursor_MemberRefExpr) {
			symbol_data.fields_name_chain.push(name);
		// Если объект яв-ся переменной, то сохраняем его имя
		} else if (kind == CXCursorKind::CXCursor_DeclRefExpr) {
			symbol_data.var_name = name;
			break;
		}
	}

	return symbol_data;
}

/**
 * Получение объекта, которому будет выполнено присваивание, с учетом того, что объект может быть размещен в стеке или в куче.
 * Присавивание всегда выполняется УКАЗАТЕЛЮ.
 */
const std::shared_ptr<Object> SymbolEvaluator::getSymbolObject(SymbolData &symbol_data, const std::string &context_name)
{
	//Сначала получаем переменную по имени
	//Затем объект, который эта переменная представляет
	auto symbol_var = analyze_functions.at(context_name)->getVarByName(symbol_data.var_name);
	//auto symbol_obj = analyze_functions.at(context_name)->getObjByAddress(symbol_var->getAddress());
	auto symbol_obj = current_program->getObjByAddress(symbol_var->getAddress());
	//Если объект яв-ся указателем и выполняется присваивание одному из полей объекта, то нужно вытащить
	//объект из кучи.
	if (symbol_obj->getType().isPointer() && !symbol_data.fields_name_chain.empty()) {
		symbol_obj = heap.getObjectByAddress(symbol_obj->getAddressVal());
	}
	//Если в symbol_data цепочка полей, к которым выполняется обращение не пустая, значит нужно получить объкт ассоциированный с этой цепочкой.
	while (!symbol_data.fields_name_chain.empty()) {
		//Получаем объект по имени поля
		symbol_obj = symbol_obj->getPtrFieldByName(symbol_data.fields_name_chain.top());
		//Если объект яв-ся указателем, то значит по адресу, который он хранит, в куче лежит нужный нам объект.
		//Если в цепочке полей, это поле яв-ся последним, значит в куче может еще ничего не лежать, поэтому извлекать объект не нужно.
		if (symbol_obj->getType().isPointer() && (symbol_data.fields_name_chain.size() > 1)) {
			symbol_obj = heap.getObjectByAddress(symbol_obj->getAddressVal());
		}
		symbol_data.fields_name_chain.pop();
	}

	return symbol_obj;
}

static void printOperator(const CXCursor &op_stmt)
{
	auto unit = clang_Cursor_getTranslationUnit(op_stmt);
	CXToken *tokens{nullptr};
	unsigned int num_tokens{0};
	tokenize(op_stmt, unit, &tokens, num_tokens);
	auto source_location = clang_getTokenLocation(unit, tokens[0]);
	CXFile file;
	unsigned int line{0};
	unsigned int column{0};
	unsigned int offset{0};
	clang_getSpellingLocation(source_location, &file, &line, &column, &offset);
	std::cout << "line " << line << ": ";
	for (decltype(num_tokens) i = 0; i < num_tokens; ++i) {
		std::cout << clang_getCString(clang_getTokenSpelling(unit, tokens[i])) << " ";
	}
	std::cout << std::endl;
	clang_disposeTokens(unit, tokens, num_tokens);
}

void SymbolEvaluator::printResult(const std::vector<FindMemoryLeakResult> &find_memory_leak_result)
{
	//for (const auto &cur : find_memory_leak_result) {
	//	std::cout << "Memory leak detected:\n\t";
	//	printOperator(cur);
	//}
	std::filesystem::path file_path;
	for (const auto &res : find_memory_leak_result) {
		file_path = getFilePathString(res.allocate_operator.first);
		std::cout << "Memory leak detected" << std::endl;
		std::cout << "First allocate in file " << file_path.filename() << " at ";
		auto allocate_stmt = res.allocate_operator.first;
		printOperator(allocate_stmt);
		if (!res.operators.empty()) {
			std::cout << "\tThis object was used in next operators:\n";
		}
		auto stmt = clang_getNullCursor();
		std::string from;
		std::string to;
		for (const auto &op : res.operators) {
			std::tie(stmt, from, to) = op;
			file_path = getFilePathString(stmt);
			std::cout << "\t" << file_path.filename() << " ";
			printOperator(stmt);
		}
		if (!clang_Cursor_isNull(res.last_operator)) {
			std::cout << "\tObject is no longer available after ";
			file_path = getFilePathString(res.last_operator);
			std::cout << file_path.filename() << " ";
			printOperator(res.last_operator);
		} else {
			std::cout << "\tMemory leak was happened when variable destroyed on stack\n";
			std::cout << "\tLast object usage ";
			auto usage_stmt = !clang_Cursor_isNull(stmt) ? stmt : res.allocate_operator.first;
			file_path = getFilePathString(usage_stmt);
			std::cout << file_path.filename() << " ";
			printOperator(usage_stmt);
		}
		/*std::cout << "Address path:\n";
		CXCursor stmt;
		std::string assigned_var;
		std::string appropriated_var;
		for (const auto &op : res.operators) {
			std::tie(stmt, assigned_var, appropriated_var) = op;
			printOperator(stmt);
			std::cout << assigned_var << " assign to " << appropriated_var << std::endl;
		}*/
	}
}

bool SymbolEvaluator::isObjectLeak(const FunDeclarations &fun_decls, const Object &heap_obj)
{
	bool res{false};

	for (const auto &[name, fun] : fun_decls) {
		for (const auto &stack_obj : fun->getStack()) {
			if (stack_obj->getType().isPointer() &&
				stack_obj->getAddressVal().getAddressValue() == heap_obj.getAddress().getAddressValue()) {
				res = true;
				break;
			}
		}
		if (res) {
			break;
		}
	}
	
	return res;
}
