#include "stdafx.h"
#include "SourceParser.h"

#include "Program.h"

void printUserTypes(const UserTypeDeclarations &ut);
void printUserType(const const std::shared_ptr<UserType> ut);
void printFunctions(const FunDeclarations &functions);
void resetBinaryOpParseDataFlags(ParseData &data);

static const std::set<std::string> INLINE_TYPES = {
    "bool",
    "short",
    "unsigned short",
    "int",
    "unsigned int",
    "long",
    "unsigned long",
    "long long",
    "unsigned long long",
    "float",
    "double",
    "char",
    "unsigned char"
};

const std::string CONST_QUALIFIER{"const"};

static std::vector<std::string> split(const std::string &orig_string, const char delimeter)
{
    std::vector<std::string> splitted_string;
    size_t cur_pos{0};
    while (cur_pos != std::string::npos) {
        auto char_pos = orig_string.find(delimeter, cur_pos);
        auto str = ((cur_pos == 0) && (char_pos == std::string::npos)) ?
                          orig_string.substr(cur_pos, orig_string.length()) :
                          (cur_pos == 0) ?              
                          orig_string.substr(cur_pos, char_pos) : 
                          (char_pos == std ::string::npos) ? 
                          orig_string.substr(cur_pos, orig_string.length() - cur_pos) :
                          orig_string.substr(cur_pos, char_pos - cur_pos);
        
        if (!str.empty()) {
            splitted_string.push_back(str);
        }
        cur_pos = (char_pos != std::string::npos)  ? (char_pos + 1) : char_pos;
    }

    return splitted_string;
}
/*
Получить вектор всех дочерних узлов, некоторого курсора.
*/
static CXChildVisitResult getAllChildrenCursor(CXCursor cursor, CXCursor parent, CXClientData data)
{
    auto v_parsed_cursor = static_cast<std::vector<CXCursor> *>(data);
    v_parsed_cursor->push_back(cursor);
    return CXChildVisit_Recurse;
}

void parseCursor(const CXCursor &cursor, std::vector<CXCursor> &cursors)
{
    clang_visitChildren(cursor, getAllChildrenCursor, (void *)&cursors);
}

void tokenize(CXCursor cursor, CXTranslationUnit &unit, CXToken **tokens, unsigned int &num_tokens)
{
    auto source_range = clang_getCursorExtent(cursor);
    unit = clang_Cursor_getTranslationUnit(cursor);
    clang_tokenize(unit, source_range, tokens, &num_tokens);
}

bool isCursorTypePointer(CXType type, int &level_indirection)
{
    bool is_pointer{false};
    auto type_str = static_cast<std::string>(clang_getCString(clang_getTypeSpelling(type)));
    bool open_sq_bracket{false};
    for (decltype(type_str.length()) i = 0; i < type_str.length(); ++i) {
        if (type_str.at(i) == '*') {
            is_pointer = true;
            ++level_indirection;
        }
        if ((type_str.at(i) == '[') && !open_sq_bracket) {
            open_sq_bracket = true;
        }
        if ((type_str.at(i) == ']') && open_sq_bracket) {
            is_pointer = true;
            open_sq_bracket = false;
            ++level_indirection;
        }
    }

    return is_pointer;
}

std::string getTypeSpelling(const CXType &type)
{
    auto raw_type_spelling = static_cast<std::string>(clang_getCString(clang_getTypeSpelling(type)));

    auto splitted_str = split(raw_type_spelling, ' ');

    auto type_spelling_it = std::find_if(splitted_str.cbegin(), splitted_str.cend(), [](const std::string &str) {
        return ((str != CONST_QUALIFIER) && (str.find('*') == std::string::npos) && (str.find('[') == std::string::npos));
        });

    return (type_spelling_it != splitted_str.cend()) ? *type_spelling_it : "";
}

bool isInternalType(const CXType &type)
{
    auto type_string = getTypeSpelling(type);

    return (INLINE_TYPES.count(type_string) > 0);
}

bool isTypeConst(const CXType &type)
{
    auto raw_type_spelling = static_cast<std::string>(clang_getCString(clang_getTypeSpelling(type)));

    auto splitted_str = split(raw_type_spelling, ' ');

    auto type_const_it = std::find_if(splitted_str.cbegin(), splitted_str.cend(), [](const std::string &str) {
        return (str == CONST_QUALIFIER);
        });
    return (type_const_it != splitted_str.cend());
}

std::string getFilePathString(const CXCursor &cursor)
{
    std::string res;
    auto unit = clang_Cursor_getTranslationUnit(cursor);
    CXToken *tokens{nullptr};
    unsigned int num_tokens = 0;
    tokenize(cursor, unit, &tokens, num_tokens);
    auto source_location = clang_getTokenLocation(unit, tokens[0]);
    CXFile file;
    unsigned int line{0};
    unsigned int column {0};
    unsigned int offset {0};
    clang_getSpellingLocation(source_location, &file, &line, &column, &offset);
    res = clang_getCString(clang_getFileName(file));
    
    clang_disposeTokens(unit, tokens, num_tokens);

    return res;
}

static bool stringIsNumber(const std::string &str)
{
    for (const auto &ch : str) {
        if (!isdigit(ch)) {
            return false;
        }
    }

    return true;
}

constexpr int UNBOUND_VAL = -1;

static int findCompoundStmtFromPos(const std::vector<CXCursor> &cursors, const int start_pos)
{
    if (start_pos > cursors.size()) return UNBOUND_VAL;

    for (decltype(cursors.size()) i = start_pos; i < cursors.size(); ++i) {
        if (clang_getCursorKind(cursors.at(i)) == CXCursorKind::CXCursor_CompoundStmt) {
            return i;
        }
    }

    return UNBOUND_VAL;
}

bool isOperatorAssign(const CXCursor &op_cursor)
{
    CXToken *tokens{nullptr};
    unsigned int num_tokens{0};
    CXTranslationUnit unit;
    tokenize(op_cursor, unit, &tokens, num_tokens);

    bool res{false};

    for (decltype(num_tokens)i = 0; i < num_tokens; ++i) {
        auto str = static_cast<std::string>(clang_getCString(clang_getTokenSpelling(unit, tokens[i])));
        if (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "=") == 0) {
            res = true;
            break;
        }
    }

    clang_disposeTokens(unit, tokens, num_tokens);

    return res;
}

std::shared_ptr<Program> SourceParser::parseSingleFileProgram(const std::string &file_name)
{
    ignored_functions.clear();
    current_program = std::make_shared<Program>(file_name);
    auto translation_unit = getTranslationUnit(file_name);

    getUserTypes(translation_unit);

    //std::cout << "All user types\n";
    //printUserTypes(current_program->getUserTypeDeclarations());

    getFunctions(translation_unit);

    //std::cout << "All functions\n";
    //printFunctions(current_program->getFunDeclarations());

    //Т.к. после парсинга исходного кода, хранить в объекте класса
    //указатель на распарсенную программу уже не нужно, надо сделать так,
    //чтобы член класса ссылающийся на объект, представляющий программу
    //перестал на него указывать. Чтобы предотвратить возможную утечку
    //памяти. Для этого сохраняем указатель во временный объект, затем
    //сбрасываем указатель класса и возвращаем временный указатель.
    decltype(current_program) returned_ptr = current_program;

    current_program.reset();

    //clang_disposeTranslationUnit(translation_unit);
    
    return returned_ptr;
}

/*
    Парсит объявление структуры или класса создавая новый пользовательский тип и помещая его в
    глобальную std::map. Помещает в него все конструкторы, которые есть у нового типа и все
    поля указатели.
*/
void SourceParser::getUserTypes(const CXTranslationUnit &unit)
{
    auto translation_unit_cursor = clang_getTranslationUnitCursor(unit);
    std::vector<CXCursor> unit_cursors;

    parseCursor(translation_unit_cursor, unit_cursors);

    for (const auto &cursor : unit_cursors) {
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_StructDecl) {
            std::vector<CXCursor> struct_decl_cursors;
            struct_decl_cursors.push_back(cursor);
            parseCursor(cursor, struct_decl_cursors);
            parseStructDecl(struct_decl_cursors);
        }
    }
}

/*
    Парсит объявление структуры.
*/
void SourceParser::parseStructDecl(const std::vector<CXCursor> &struct_decl_cursors)
{
    current_user_type = std::make_shared<UserType>();

    if (!isStructDeclEffectOnHeap(struct_decl_cursors)) {
        current_user_type->setTypeName(clang_getCString(clang_getCursorSpelling(struct_decl_cursors.at(0))));
        current_user_type->setDummy(true);
        if (current_program && current_program->addUserType(current_user_type)) {
            //std::cout << current_user_type->getTypeName() << " was inserted in program as dummy\n";
        }
        else {
            //std::cout << current_user_type->getTypeName() << " also inserted in program\n";
        }
        return;
    }

    current_user_type->setTypeName(clang_getCString(clang_getCursorSpelling(struct_decl_cursors.at(0))));
    current_user_type->setDummy(false);

    //Парсинг полей структуры
    int type_size{0};
    for (const auto &cursor : struct_decl_cursors) {
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_FieldDecl) {
            Variable field;
            int field_size = 1;
            if (isStructFieldEffectOnHeap(cursor, field, field_size)) {
                current_user_type->addField(field);
                type_size += field_size;
            }
        }
    }
    current_user_type->setSize(type_size);

    //Парсинг конструктора структуры
    for (const auto &cursor : struct_decl_cursors) {
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_Constructor) {
            CXCursor constructor_cursor = clang_getNullCursor();
            if (isFunctionDefinition(cursor)) {
                constructor_cursor = cursor;
            } else if (!findConstructorDefinition(cursor, constructor_cursor)) {
                continue;
            }
            std::vector<CXCursor> constructor_decl;
            constructor_decl.push_back(constructor_cursor);
            parseCursor(constructor_cursor, constructor_decl);
            Constructor constructor = parseStructConstructor(constructor_decl);
            current_user_type->addConstructor(constructor);
        }
    }

    if (current_user_type->getConstructors().empty()) {
        current_user_type->addConstructor(Constructor(true));
    }

    decltype(current_user_type)user_type = current_user_type;

    current_user_type.reset();

    if (current_program && current_program->addUserType(user_type)) {
        //std::cout << user_type->getTypeName() << " was inserted in program\n";
    } else {
        //std::cout << user_type->getTypeName() << " also inserted in program\n";
    }
    //analyzeHelper.printUserType(current_user_type);
}

bool SourceParser::isStructFieldEffectOnHeap(CXCursor cursor_field, Variable &field, int &field_size)
{
    int level_indirection{0};
    auto cursor_type = clang_getCursorType(cursor_field);
    Address addr{-1};
    auto is_ptr = isCursorTypePointer(cursor_type, level_indirection);
    auto is_const = isTypeConst(cursor_type);
    auto type_name = getTypeSpelling(cursor_type);
    auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(cursor_field)));
    if (is_ptr) {
        field.setName(name);
        field.setType(Type(type_name, is_const, is_ptr, level_indirection, field_size));
        field.setAddress(addr);
        return true;
    } else if (current_program->hasUserType(type_name)
        && isUserTypeEffectOnHeap(cursor_type)) {
        field_size = current_program->getUserTypeByName(type_name)->getSize();
        field.setName(name);
        field.setType(Type(type_name, is_const, is_ptr, level_indirection, field_size));
        field.setAddress(addr);
        return true;
    }
    //Variable pointer_var(name, Type(type_name, is_const, is_ptr, level_indirection), addr);
    return false;
}

/*
    Парсит конструктор, возвращая объек типа Constructor. Помещает в него
    аргументы указатели и тело конструктора в виде вектора курсоров.
*/
Constructor SourceParser::parseStructConstructor(const std::vector<CXCursor> &constructor_decl)
{
    Constructor constructor;

    constructor.setName(clang_getCString(clang_getCursorSpelling(constructor_decl.at(0))));

    //получаю тело конструктора и сохраняю курсор на его начало
    std::vector<CXCursor> body;
    CXCursor cmpnd_stmt_cursor;
    bool body_start{false};
    for (const auto &op : constructor_decl) {
        if (body_start) body.push_back(op);
        if (!body_start && (clang_getCursorKind(op) == CXCursorKind::CXCursor_CompoundStmt)) { 
            cmpnd_stmt_cursor = op;
            body.push_back(op);
            body_start = true; 
        }
    }
    constructor.setBody(body);

    //Выполняется парсинг списка инициализаторов членов конструктора в структуру,
    //определенную в классе конструктора. В нее заносятся только те выражения, которые
    //инициализируют поля, оказывающие влияние на кучу.
    Constructor::InitializeList initialize_list;
    bool init_list_start{false};
    std::string current_member;
    std::vector<CXCursor> init_exprs;
    for (const auto &op : constructor_decl) {
        if (clang_getCursorKind(op) == CXCursorKind::CXCursor_CompoundStmt) {
            if (!init_exprs.empty()) {
                initialize_list.push_back(std::make_pair(current_member, init_exprs));
            }
            break;
        }
        if ((clang_getCursorKind(op) != CXCursorKind::CXCursor_MemberRef) && init_list_start) {
            if (clang_getCursorKind(op) != CXCursorKind::CXCursor_UnexposedExpr) {
                init_exprs.push_back(op);
            }
        }
        if (clang_getCursorKind(op) == CXCursorKind::CXCursor_MemberRef) {
            if (!init_exprs.empty()) {
                initialize_list.push_back(std::make_pair(current_member, init_exprs));
            }
            if (!current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op)))) {
                init_list_start = false;
                init_exprs.clear();
                current_member.clear();
                continue;
            }
            init_list_start = true;
            init_exprs.clear();
            current_member.clear();
            current_member = clang_getCString(clang_getCursorSpelling(op));
        }
    }

    for (const auto &[name, exprs] : initialize_list) {
        std::cout << name << ": ";
        for (const auto &ex : exprs) {
            std::cout << clang_getCString(clang_getCursorSpelling(ex)) << " ";
        }
        std::cout << std::endl;
    }

    constructor.setInitializeList(initialize_list);
    //TODO:
    //не учитывает список инициализации;
    //если у конструктора все его параметры яв-ся параметрами по умолчанию,
    //то такой конструктор нужно считать конструктором по умолчанию;
    auto param_num = clang_Cursor_getNumArguments(constructor_decl.at(0));
    for (decltype(param_num) i = 0; i < param_num; ++i) {
        auto param = clang_Cursor_getArgument(constructor_decl.at(0), i);
        int counter{0};
        auto result = ParseResult::None;
        auto param_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(param)));
        //std::cout << clang_getCString(clang_getCursorSpelling(param)) << " " << isArgEffectOnHeap(cmpnd_stmt_cursor, 0, param_name, result, counter) << std::endl;
        //ParseData data;
        //data.current_repr = clang_getCString(clang_getCursorSpelling(param));
        //parseOperator(cmpnd_stmt_cursor, 0, data.current_repr, data, counter);
        //data.is_arg_effect_on_heap
        CXType param_type = clang_getCursorType(param);
        int level_indirection{0};
        int value{-1};
        auto is_ptr = isCursorTypePointer(param_type, level_indirection);
        auto is_const = isTypeConst(param_type);
        auto type_name = getTypeSpelling(param_type);
        auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(param)));
        int size = (is_ptr && (level_indirection > 1)) ? 1 : current_program->hasUserType(type_name)
            ? current_program->getUserTypeByName(type_name)->getSize() : 1;
        Type type{type_name, is_const, is_ptr, level_indirection, size};
        Object object;
        object.setType(type);
        CallableObject::Parameter constructor_param;
        if (isArgInInitList(param, initialize_list)
            || is_ptr) {
            if (getParamDefaultValueIfItHave(param, value)) {
                if (type.isPointer()) {
                    object.setAddressVal(Address(value));
                } else if (type.getTypeName() == "int") {
                    object.setIntVal(value);
                } else if (type.getTypeName() == "bool") {
                    object.setBoolVal(value);
                }
            }
            constructor_param.argument = Variable{name, type};
            constructor_param.default_val = object;
            constructor_param.is_used = true;
            constructor_param.num = i;
            constructor.addParam(constructor_param);
        } else {
            constructor_param.argument = Variable{name, type};
            constructor_param.default_val = object;
            constructor_param.is_used = false;
            constructor_param.num = i;
            constructor.addParam(constructor_param);
        }
    }

    return constructor;
}

bool SourceParser::findConstructorDefinition(CXCursor constructor_decl, CXCursor &constructor_def)
{
    auto current_unit = clang_Cursor_getTranslationUnit(constructor_decl);
    std::vector<CXCursor> unit_cursors;
    parseCursor(clang_getTranslationUnitCursor(current_unit), unit_cursors);
    for (const auto &cursor : unit_cursors) {
        if ((clang_getCursorKind(cursor) == CXCursor_Constructor)
            && (strcmp(clang_getCString(clang_getCursorSpelling(cursor)), clang_getCString(clang_getCursorSpelling(constructor_decl))) == 0)
            && isFunctionDefinition(cursor)) {
            constructor_def = cursor;
            return true;
        }
    }

    return false;
}

bool SourceParser::isArgInInitList(CXCursor param, const std::vector<std::pair<std::string, std::vector<CXCursor> > > &init_list)
{
    for (const auto &[mem_name, exprs] : init_list) {
        for (const auto &exp : exprs) {
            if (strcmp(clang_getCString(clang_getCursorSpelling(param)), clang_getCString(clang_getCursorSpelling(exp))) == 0) {
                return true;
            }
        }
    }

    return false;
}


//
bool SourceParser::isArgEffectOnHeap(const CXCursor &op_cursor, const int recursion_depth, std::string &arg_name, ParseResult &result, int &op_counter)
{
    std::vector<CXCursor> op_cursors;
    parseCursor(op_cursor, op_cursors);

    auto context = getContextType(op_cursor);

    auto op_type = getOperatorType(op_cursor);
    BinaryOperatorType bin_op_type = BinaryOperatorType::None;

    if (op_type == OperatorType::BinaryOperator) {
        bin_op_type = getBinaryOperatorType(op_cursor);
    }

    int dummy{0};

    bool left_value{true};
    bool field_find{false};
    bool field_ptr_find{false};
    bool field_field_find{false};
    bool var_find{false};
    bool var_ptr_find{false};
    bool arg_ptr_find{false};

    for (unsigned int i = 0; i < op_cursors.size(); i++) {
        ++op_counter;
        auto kind = clang_getCursorKind(op_cursors.at(i));
        auto type = clang_getCursorType(op_cursors.at(i));
        std::string spelling = clang_getCString(clang_getCursorSpelling(op_cursors.at(i)));
        if (kind == CXCursorKind::CXCursor_BinaryOperator) {
            int counter = 0;
            //ParseResult res = ParseResult::None;
            //std::string new_name = arg_name;
            isArgEffectOnHeap(op_cursors.at(i), recursion_depth + 1, arg_name, result, counter);
            op_counter += counter;
            i = op_counter - 1;
            if (context == ContextType::Constructor) {
                if (result == ParseResult::ArgAssignToStructField) {
                    return true;
                } else if (result == ParseResult::ArgPtrAssignToStructFieldPtr) {
                    return true;
                } else if (result == ParseResult::ArgAssignToFieldStructField) {
                    return true;
                } else if (result == ParseResult::ArgAssignToVar) {
                    continue;
                } else if (result == ParseResult::ArgPtrAssignToVarPtr) {
                    continue;
                } else if (result == ParseResult::VarAssignToArg) {
                    return true;
                } else if (result == ParseResult::VarPtrAssignToArgPtr) {
                    return true;
                }
            } else if ((context == ContextType::BinaryOperator) && (bin_op_type == BinaryOperatorType::Assign)) {
                if (field_find) {
                    if (result == ParseResult::ArgAssignToVar) {
                        result = ParseResult::ArgAssignToStructField;
                        return true;
                    } else if (result == ParseResult::VarAssignToArg) {
                        return true;
                    }
                } else if (field_ptr_find) {
                    if (result == ParseResult::ArgPtrAssignToVarPtr) {
                        result = ParseResult::ArgPtrAssignToStructFieldPtr;
                        return true;
                    } else if (result == ParseResult::VarPtrAssignToArgPtr) {
                        return true;;
                    }
                } else if (field_field_find) {
                    if (result == ParseResult::ArgPtrAssignToVarPtr) {
                        result = ParseResult::ArgAssignToFieldStructField;
                        return true;
                    } else if (result == ParseResult::VarPtrAssignToArgPtr) {
                        result = ParseResult::ArgAssignToStructField;
                        return true;
                    }
                } else if (var_ptr_find) {
                    if (result == ParseResult::FieldAssignToArgPtr) {
                        return true;
                    }
                }
            }
        }

        if (op_type == OperatorType::BinaryOperator) {
            if (bin_op_type == BinaryOperatorType::Assign) {
                if (left_value && (kind == CXCursorKind::CXCursor_MemberRefExpr)
                    && (!isCursorTypePointer(type, dummy))
                    && (isInternalType(type))
                    && (current_user_type->hasField(spelling)))
                {
                    left_value = false;
                    field_find = true;
                    continue;
                } else if (left_value && (kind == CXCursorKind::CXCursor_MemberRefExpr)
                    && (isCursorTypePointer(type, dummy))
                    && (current_user_type->hasField(spelling)))
                {
                    left_value = false;
                    field_ptr_find = true;
                    continue;
                }/* else if (left_value && ((kind == CXCursorKind::CXCursor_MemberRefExpr)
                    && ((clang_getCursorKind(op_cursors.at(i + 1)) == CXCursorKind::CXCursor_MemberRefExpr)
                    && (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i + 1)))))
                        || (clang_getCursorKind(op_cursors.at(i + 2)) == CXCursorKind::CXCursor_MemberRefExpr)
                        && (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i + 2)))))))
                    && (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))) {
                    left_value = false;
                    field_field_find = true;
                    continue;
                }*/else if (left_value && (kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) != 0)
                    && (!isCursorTypePointer(type, dummy))
                    && (isInternalType(type)))
                {
                    left_value = false;
                    var_find = true;
                    continue;
                } else if (left_value && ((kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) != 0))
                    && (isCursorTypePointer(clang_getCursorType(op_cursors.at(i)), dummy))) {
                    left_value = false;
                    var_ptr_find = true;
                    continue;
                } else if (left_value && ((kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) == 0))
                    && (isCursorTypePointer(clang_getCursorType(op_cursors.at(i)), dummy)))
                {
                    left_value = false;
                    arg_ptr_find = true;
                }

                if (field_find && (kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) == 0)) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    result = ParseResult::ArgAssignToStructField;
                    return true;
                } else if (field_ptr_find && (kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) == 0)) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    result = ParseResult::ArgPtrAssignToStructFieldPtr;
                    return true;
                }/* else if (field_field_find && (kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) == 0)) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    result = ParseResult::ArgAssignToFieldStructField;
                    return true;
                }*/else if (var_find && (kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) == 0)) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    arg_name = spelling;
                    result = ParseResult::ArgAssignToVar;
                    return true;
                } else if (var_ptr_find && (kind == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(spelling.c_str(), arg_name.c_str()) == 0)) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    arg_name = spelling;
                    result = ParseResult::ArgPtrAssignToVarPtr;
                    return true;
                } else if (arg_ptr_find && (kind == CXCursorKind::CXCursor_DeclRefExpr)) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    result = ParseResult::VarPtrAssignToArgPtr;
                    return true;
                } else if (arg_ptr_find && (kind == CXCursorKind::CXCursor_MemberRefExpr)
                    && (current_user_type->hasField(spelling))) {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    result = ParseResult::FieldAssignToArgPtr;
                    return true;
                }
            }
        }
    }

    return false;
}

/*
 * Рекурсивно парсит оператор(ы) проверяя оказывает ли влияние переменная(объект) arg_name влияние на кучу.
 * Решение, что переменная влияет на кучу принимается в следующих случаях:
 *  - полю структуры или переменной, которое оказывает влияние на кучу был присвоен рассматриваемый аргумент;
 *  - полю структуры, объект которой является членом другой структуры, и которое(поле) оказывает влияние на кучу, был присвоен рассматриваемый аргумент;
 *  - аргумент был использован как параметр функции или конструктора, в которых он влияет на кучу;
 *  - аргумент был использован в операторе new, для встроенных и пользовательских типов при рамещении массивов;
 *  - аргумент был использован в операторе new для размещения в куче пользовательских типов и передан в конструктор параметром, который оказывате влияние на кучу;
 * Присвоение переменной, не являющейся полем класса, явно не обрабатывается, т.к., если в левой части оператора 
*/
void SourceParser::parseOperator(const CXCursor &op_cursor, const int recursion_depth, const std::string &arg_name, ParseData &data, int &op_counter)
{
    auto context = std::make_shared<Context>();
    std::vector<CXCursor> op_cursors;
    parseCursor(op_cursor, op_cursors);

    //Определяется тип, разворачиваемого, для парсинга оператора
    //Елси это бинарный тип, то определяем его тип. Нас интересуют
    //присваивание, объявление с присваиванием, сравнение и арифметический
    //операторы. Последние нужно проверить, чтобы узнать было ли
    //передано в бинарном операторе значение рассматриваемого
    //аргумента другому объекту, чтобы продолжить наблюдать уже за ним.
    //Также может быть вызываюзий оператор, например new или вызов
    //функции.
    OperatorType op_type = getOperatorType(op_cursor);
    BinaryOperatorType bin_op_type = BinaryOperatorType::None;
    CallExprType call_expr_type = CallExprType::None;
    if (op_type == OperatorType::BinaryOperator) {
        bin_op_type = getBinaryOperatorType(op_cursor);
    } else if (op_type == OperatorType::CallExpr) {
        call_expr_type = getOperatorCallExprType(op_cursor);
    } else if (op_type == OperatorType::DeclAssign) {
        bin_op_type = BinaryOperatorType::Assign;
    }

    //Для парсинга оператора присваивания
    //Эти флаги используются для определения левой
    //и правой частей оператора присваивания и
    //того, что оказалось в этих частях(поле, поле структуры,
    //поле структуры локально объявленного объекта или переменная).
    //Флаг переменной означает, что в операторе присваивания
    //аргумент был присвоен другой перменной и теперь необходимо
    //следить за ней. Ее имя сохраняется в next_var.
    bool left_value = true;
    bool field_find = false;
    bool var_field_another_ut_field_find = false;
    bool field_another_ut_field_find = false;
    bool arg_to_next = false;
    bool arg_assign = false; //при условии, что аргумент - указатель
    std::string next_var;

    int dummy; //фиктивная переменная для ф-ции isCursorTypePointer

    //для парсинга операторов объявления и new с вызовом конструкторов
    std::vector<Constructor> constructors;
    std::vector<Function> functions;
    unsigned int num_args = 0;
    unsigned int current_arg_num = 0;

    if (op_type == OperatorType::CallExpr) {
        if (call_expr_type == CallExprType::NewExprBaseType) {
            data.is_new_base = true;
            op_counter += op_cursors.size();
            return;
        } else if (call_expr_type == CallExprType::NewExprUserType) {
            data.is_new_ut = true;
            op_counter += op_cursors.size();
            return;
        } else if (call_expr_type == CallExprType::FunctionExpr) {
            num_args = clang_Cursor_getNumArguments(op_cursor);
            functions = current_program->getFunctionsByNumArgs(num_args);
        }
    }

    bool count = false;
    
    for (size_t i = 0; i < op_cursors.size(); i++) {
        if (recursion_depth == 0) {
            resetBinaryOpParseDataFlags(data);
        }
        ++op_counter;
        
        if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_IfStmt) {
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
       /*1*/op_counter += counter;
            i = op_counter - 1;
            if (data.is_arg_used && (data.is_new_base || data.is_new_base_arr || data.is_new_base_paren
                || data.is_new_ut || data.is_new_ut_arr || data.is_new_ut_paren || data.is_delete/* || data.is_arg_assign*/)) {
                data.is_arg_effect_on_heap = true;
                return;
            } else if (data.is_arg_used && (data.is_field_assigned || data.is_var_another_ut_field_assigned || data.is_another_ut_field_from_field_find)) {
                data.is_arg_effect_on_heap = true;
                return;
            }
            //resetBinaryOpParseDataFlags(data);
            continue;
        }
        if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_ForStmt)
            || (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_WhileStmt)) {
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;
            if (data.is_arg_used && (data.is_new_base || data.is_new_base_arr || data.is_new_base_paren
                || data.is_new_ut || data.is_new_ut_arr || data.is_new_ut_paren || data.is_fun || data.is_delete /*|| data.is_arg_assign*/)) {
                data.is_arg_effect_on_heap = true;
                return;
            } else if (data.is_arg_used && (data.is_field_assigned || data.is_var_another_ut_field_assigned || data.is_another_ut_field_from_field_find)) {
                data.is_arg_effect_on_heap = true;
                return;
            }
            //resetBinaryOpParseDataFlags(data);
            continue;
        }
        if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_CompoundStmt) {
            int counter = 0;
       /*2*/parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;
            return;
        }
        if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclStmt) {
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;
            if (data.is_arg_used && (data.is_new_base_arr || data.is_new_ut_arr
                || data.is_new_ut_paren || data.is_fun))
            {
                data.is_arg_effect_on_heap = true;
                return;
            }
            //если data.next_var == true, то тогда продолжается поиск оператора, оказывающего влияние на кучу
            //в этом случае значение data.current_repr изменяется, поэтому возвращаться не нужно, и этот if пропущен
            //if(data.next_var && data.is_arg_used) {
            //  continue
            //}
            continue;
        }
        //Вход в рекурсию и обработка результата
        if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_BinaryOperator) {
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;

            if (data.is_arg_used && (data.is_new_base || data.is_new_base_arr || data.is_new_base_paren
                || data.is_new_ut || data.is_new_ut_arr || data.is_new_ut_paren || data.is_fun)) {
                data.is_arg_effect_on_heap = true;
                return;
            } else if (data.is_arg_used && (data.is_field_assigned || data.is_var_another_ut_field_assigned 
                || data.is_another_ut_field_from_field_find)) {
                data.is_arg_effect_on_heap = true;
                return;
            }
            
            if ((field_find && data.is_arg_used)
                || (field_find && data.is_arg_used && data.is_next_var)) {
                data.is_field_assigned = true;
                return;
            }/*else if (arg_assign && (data.is_new_base || data.is_new_base_arr || data.is_new_base_paren
                || data.is_new_ut || data.is_new_ut_arr || data.is_new_ut_paren || data.is_fun)) {
                data.is_arg_assign = true;
                return;
            }*/ else if ((var_field_another_ut_field_find && data.is_arg_used)
                || (var_field_another_ut_field_find && data.is_arg_used && data.is_next_var)) {
                data.is_var_another_ut_field_assigned = true;
                return;
            } else if ((field_another_ut_field_find && data.is_arg_used)
                || (field_another_ut_field_find && data.is_arg_used && data.is_next_var)) {
                data.is_another_ut_field_from_field_find = true;
                return;
            } else if (arg_to_next && data.is_arg_used) {
                data.is_next_var = true;
                data.current_repr = next_var;
            } else if (data.is_next_var) {
                data.is_next_var = false;
            }
            continue;
        }
        if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_CXXNewExpr) {
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;
            /*
            if(parent_context->data.is_arg_used
            */
            if (data.is_arg_used && (data.is_new_base_arr
                || data.is_new_ut_arr || data.is_new_ut_paren))
            {
                return;
            }// else if (arg_to_next && ())
            continue;
        }
        if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_CXXDeleteExpr) {
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;
            if (data.is_arg_used && data.is_delete) {
                return;
            }
            continue;
        }
        if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_CallExpr)
            && ((call_expr_type != CallExprType::NewExprUserType) || (call_expr_type != CallExprType::NewExprUserTypeArr)
                || (call_expr_type != CallExprType::NewExprUserTypeParen))) {
            if (!current_program->hasFunction(clang_getCString(clang_getCursorSpelling(op_cursors.at(i))))) {
                getFunction(op_cursors.at(i));
                if (!current_program->hasFunction(clang_getCString(clang_getCursorSpelling(op_cursors.at(i))))) {
                    return;
                }
            }
            int counter = 0;
            parseOperator(op_cursors.at(i), recursion_depth + 1, data.current_repr, data, counter);
            op_counter += counter;
            i = op_counter - 1;
            if (data.is_arg_used && data.is_fun) {
                return;
            }
        }

        //Обработка операторов
        if (op_type == OperatorType::BinaryOperator || op_type == OperatorType::DeclAssign) {
            if (bin_op_type == BinaryOperatorType::Assign) {
                //Проверка чем является левая часть оператора присваивание
                //  - полем класса (структуры)
                //  - полем поля структуры
                //  - полем поля переменной пользов. типа (стр-ры, класса)
                //  - указателем (любым)
                //  - переменной (любого типа)
                //  - аргументом передаваемым в конструктор (функцию, метод), которому может быть присвоено значение (считаем такими аргументами, аргументы типа указатель)
                //  - аргументом передаваемым в конструктор (функцию, метод), которому НЕ может быть присвоено значение (аргументы, не яв-ся указателями)
                if (left_value && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_MemberRefExpr)
                    && current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))
                {
                    field_find = true;
                    left_value = false;
                    continue;
                } else if (left_value && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_MemberRefExpr)
                    //Проверка, яв-ся ли левая часть полем поля стр-ры(класса). Вторая проверка, через || для обращения к полю поля, типа указатель
                    && ((current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i + 1))))
                        && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 1))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))
                        || (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i + 2))))
                            && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 2))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))))
                {
                    left_value = false;
                    field_another_ut_field_find = true;
                    continue;
                } else if (left_value && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_MemberRefExpr)
                    //Проверка, яв-ся ли левая часть переменной пользов. типа, полю которой будет выполнятся присваивание. Вторая проверка, через || для обращения к полю переменной, яв-ся указателем.
                    && ((current_program->hasUserType(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 1))))
                        && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 1))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))
                        || (current_program->hasUserType(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 2))))
                        && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 2))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))))
                {
                    left_value = false;
                    var_field_another_ut_field_find = true;
                    continue;
                }else if (left_value && ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    //Проверка, на то, что левая часть НЕ указатель
                    || (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_VarDecl))
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) != 0)
                    /*&& (!isCursorTypePointer(clang_getCursorType(op_cursors.at(i)), dummy))*/)
                {
                    left_value = false;
                    arg_to_next = true;
                    next_var = clang_getCString(clang_getCursorSpelling(op_cursors.at(i)));
                    continue;
                } /*else if (left_value && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    //Проверка на то, яв-ся ли левая часть аргументом, который в свою очередь - указатель
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0)
                    && (isCursorTypePointer(clang_getCursorType(op_cursors.at(i)), dummy)))
                {***********************************************************
                    left_value = false;
                    arg_assign = true;
                    continue;
                } */else if (left_value && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    //Если левая часть аргумент и он не указатель, выходим из ф-ции.
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0)
                    /*&& (!isCursorTypePointer(clang_getCursorType(op_cursors.at(i)), dummy))*/)
                {
                    left_value = false;
                    data.is_arg_used = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
            
                //Проверка чем является правая часть оператора присваивание
                //  - аргументом передаваемым в конструктор (функцию, метод), с учетом того, что в левой части поле класса(структуры)
                //  - аргументом передаваемым в конструктор (функцию, метод), с учетом того, что в левой части переменная пользов. типа, к полю которого вып-ся обращение
                //  - аргументом передаваемым в конструктор (функцию, метод), с учетом того, что в левой части поле другой структуры, которая яв-ся полем текущего класса(стр-ры)
                //  - аргументом, с учетом того, что в левой части перменная НЕ указатель
                //  - аргументом, с учетом того, что в левой части переменная указатель
                //  - полем стр-ры, полем поля стр-ры, полем поля переменной пользов. типа, переменной и все они указатели, присваиваемые аргументу указателю
                //  - не яв-ся аргументом, с учетом того, что в левой части переменная НЕ указатель
                //  - не яв-ся аргументом, с учетом того, что в левой части указатель
                if (field_find && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    data.is_field_assigned = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                } else if (var_field_another_ut_field_find && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    data.is_var_another_ut_field_assigned = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                } else if (field_another_ut_field_find && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    data.is_another_ut_field_from_field_find = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                } else if (arg_to_next && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.current_repr = next_var;
                    data.is_next_var = true;
                    data.is_arg_used = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }/* else if (arg_assign && ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    //Является ли правая часть полем класса
                    || ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_MemberRefExpr) && (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i))))
                        //Проверка на то, является ли правая часть, присваиваемая переданному в ф-цию аргументу, полем поля стр-ры(класса)
                        || (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i + 1))))
                            && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 1))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))
                        || (current_user_type->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i + 2))))
                            && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 2))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))
                        ////Проверка на то, является ли правая часть, присваиваемая переданному в ф-цию аргументу, полем поля переменной пользов. типа (стр-ры, класса)
                        || (current_program->hasUserType(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 1))))
                            && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 1))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))))
                        || (current_program->hasUserType(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 2))))
                            && current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i + 2))))->hasField(clang_getCString(clang_getCursorSpelling(op_cursors.at(i))))))))
                    //Является ли правая часть указателем
                    && (isCursorTypePointer(clang_getCursorType(op_cursors.at(i)), dummy)))
                {
                    data.is_arg_used = true;
                    data.is_arg_assign = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }*/ else if (arg_to_next && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) != 0))
                {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                } /*else if (ptr_assign && (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) != 0))
                {
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }*/
            } else if (bin_op_type == BinaryOperatorType::Arithmetic) {
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
            } else if (bin_op_type == BinaryOperatorType::Comparsion) {
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
            }
        } else if (op_type == OperatorType::CallExpr) {
            if (call_expr_type == CallExprType::NewExprBaseTypeArr) {
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    data.is_new_base_arr = true;
                    data.is_arg_effect_on_heap = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
            } else if (call_expr_type == CallExprType::NewExprBaseTypeParen) {
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_new_base_paren = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    data.is_arg_effect_on_heap = true;
                    return;
                }
            } else if (call_expr_type == CallExprType::NewExprUserTypeArr) {
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0))
                {
                    data.is_arg_used = true;
                    data.is_new_ut_arr = true;
                    data.is_arg_effect_on_heap = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
            } else if (call_expr_type == CallExprType::NewExprUserTypeParen) {
                if (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_CallExpr) {
                    num_args = clang_Cursor_getNumArguments(op_cursors.at(i));
                    constructors = current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursors.at(i))))->getConstructorsByNumArgs(num_args);
                }
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0)
                    && current_program->hasUserType(getTypeSpelling(clang_getCursorType(op_cursor)))
                    && !current_program->getUserTypeByName(getTypeSpelling(clang_getCursorType(op_cursor)))->isDummy()
                    && isConstructorArgMatchAndUse(op_cursors.at(i), current_arg_num, constructors))
                {
                    data.is_arg_used = true;
                    data.is_new_ut_paren = true;
                    data.is_arg_effect_on_heap = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
                (clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr) ? ++current_arg_num : current_arg_num;
            } else if (call_expr_type == CallExprType::FunctionExpr) {
                if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                    && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0)
                    && current_program->hasFunction(clang_getCString(clang_getCursorSpelling(op_cursor)))
                    && isFunctionArgMatchAndUse(op_cursors.at(i), current_arg_num, functions))
                {
                    data.is_arg_used = true;
                    data.is_fun = true;
                    data.is_arg_effect_on_heap = true;
                    op_counter = op_counter + (op_cursors.size() - op_counter);
                    return;
                }
            }
        } else if (op_type == OperatorType::DeleteExpr) {
            if ((clang_getCursorKind(op_cursors.at(i)) == CXCursorKind::CXCursor_DeclRefExpr)
                && (strcmp(arg_name.c_str(), clang_getCString(clang_getCursorSpelling(op_cursors.at(i)))) == 0)) {
                data.is_arg_used = true;
                data.is_delete = true;
                data.is_arg_effect_on_heap = true;
                op_counter = op_counter + (op_cursors.size() - op_counter);
                return;
            }
        }
        
    }
    //return data.is_arg_used;
}

bool SourceParser::isConstructorArgMatchAndUse(CXCursor arg, unsigned int num, const std::vector<Constructor> &constructors)
{
    auto arg_type = getTypeSpelling(clang_getCursorType(arg));
    for (const auto &con : constructors) {
        unsigned int arg_num{0};
        for (const auto &arg : con.getSignature()) {
            if ((arg.argument.getType().getTypeName() == arg_type)
                && arg.is_used && (arg_num == num)) {
                return true;
            }
            ++arg_num;
        }
    }

    return false;
}

bool SourceParser::isFunctionArgMatchAndUse(CXCursor arg, unsigned int num, const std::vector<Function> &functions)
{
    auto arg_type = getTypeSpelling(clang_getCursorType(arg));
    for (const auto &fun : functions) {
        unsigned int arg_num{0};
        for (const auto &arg : fun.getSignature()) {
            if ((arg.argument.getType().getTypeName() == arg_type)
                && arg.is_used && (arg_num == num)) {
                return true;
            }
            ++arg_num;
        }
    }

    return false;
}

OperatorType SourceParser::getOperatorType(const CXCursor &op_cursor)
{
    auto op_type = OperatorType::None;

    auto kind = clang_getCursorKind(op_cursor);
    if (kind == CXCursorKind::CXCursor_BinaryOperator) {
        op_type = OperatorType::BinaryOperator;
    } else if (kind == CXCursorKind::CXCursor_DeclStmt
        && isOperatorAssign(op_cursor))
    {
        op_type = OperatorType::DeclAssign;
    } else if (kind == CXCursorKind::CXCursor_CallExpr
        || kind == CXCursorKind::CXCursor_CXXNewExpr
        || kind == CXCursorKind::CXCursor_CXXMethod)
    {
        op_type = OperatorType::CallExpr;
    } else if (kind == CXCursorKind::CXCursor_CXXDeleteExpr) {
        op_type = OperatorType::DeleteExpr;
    }
    
    return op_type;
}

CallExprType SourceParser::getOperatorCallExprType(const CXCursor &call_expr_cursor)
{
    auto is_base_type = isInternalType(clang_getCursorType(call_expr_cursor));
    CXToken *tokens{nullptr};
    unsigned int num_tokens{0};
    CXTranslationUnit unit;
    tokenize(call_expr_cursor, unit, &tokens, num_tokens);

    auto res = CallExprType::None;

    bool is_paren{false};
    bool is_square_bracket{false};
    for (decltype(num_tokens) i = 0; i < num_tokens; ++i) {
        if (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "[") == 0) {
            is_square_bracket = true;
            break;
        } else if (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "(") == 0) {
            is_paren = true;
            break;
        }
    }
    if (is_base_type && is_paren) {
        res = CallExprType::NewExprBaseTypeParen;
    } else if (is_base_type && is_square_bracket) {
        res = CallExprType::NewExprBaseTypeArr;
    } else if (is_base_type) {
        res = CallExprType::NewExprBaseType;
    } else if (!is_base_type && is_paren) {
        res = CallExprType::NewExprUserTypeParen;
    } else if (!is_base_type && is_square_bracket) {
        res = CallExprType::NewExprUserTypeArr;
    } else if (!is_base_type) {
        res = CallExprType::NewExprUserType;
    }

    clang_disposeTokens(unit, tokens, num_tokens);

    return res;
}

BinaryOperatorType SourceParser::getBinaryOperatorType(const CXCursor &bin_op_cursor)
{
    CXToken *tokens{nullptr};
    unsigned int num_tokens{0};
    CXTranslationUnit unit;
    tokenize(bin_op_cursor, unit, &tokens, num_tokens);

    auto result = BinaryOperatorType::None;

    for (decltype(num_tokens) i = 0; i < num_tokens; ++i) {
        if (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "=") == 0) {
            result = BinaryOperatorType::Assign;
            break;
        } else if ((strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "+") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "-") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "*") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "/") == 0)) {
            result = BinaryOperatorType::Arithmetic;
            break;
        } else if ((strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), ">") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "<") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), ">=") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "<=") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "==") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "!=") == 0)) {
            result = BinaryOperatorType::Comparsion;
            break;
        } else if ((strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "&&") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "||") == 0)
            || (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "!") == 0)) {
            result = BinaryOperatorType::Logical;
            break;
        }
    }

    clang_disposeTokens(unit, tokens, num_tokens);

    return result;
}

ContextType SourceParser::getContextType(const CXCursor &cursor)
{
    auto res = ContextType::None;

    auto kind = clang_getCursorKind(cursor);
    if (kind == CXCursorKind::CXCursor_CompoundStmt) {
        res = ContextType::Compound;
    } else if (kind == CXCursorKind::CXCursor_BinaryOperator) {
        res = ContextType::BinaryOperator;
    }

    return res;
}

/*
    Проверяет является ли параметр, на который указывает курсор param
    параметром со значением по умолчанию и, если это так, то записывает
    значение в value. Значением является адрес.
*/
bool SourceParser::getParamDefaultValueIfItHave(CXCursor param, int &value)
{
    auto source_range = clang_getCursorExtent(param);
    CXToken *tokens{nullptr};
    unsigned int num_tokens{0};
    auto unit = clang_Cursor_getTranslationUnit(param);
    clang_tokenize(unit, source_range, &tokens, &num_tokens);
    for (decltype(num_tokens) i = 0; i < num_tokens; ++i) {
        auto str = static_cast<std::string>(clang_getCString(clang_getTokenSpelling(unit, tokens[i])));
        if (strcmp(clang_getCString(clang_getTokenSpelling(unit, tokens[i])), "=") == 0) {
            auto str_value = static_cast<std::string>(clang_getCString(clang_getTokenSpelling(unit, tokens[i + 1])));
            if (str_value == "nullptr" || str_value == "NULL" || str_value == "false") {
                value = 0;
                return true;
            } else if (str_value == "true") {
                value = 1;
                return true;
            } else if (clang_getTokenKind(tokens[i + 1]) == CXTokenKind::CXToken_Literal
                        && stringIsNumber(str_value)) {
                value = atoi(str_value.c_str());
                return true;
            }
        }
    }

    clang_disposeTokens(unit, tokens, num_tokens);
    return false;
}

void SourceParser::getFunctions(const CXTranslationUnit &unit)
{
    auto translation_unit_cursor = clang_getTranslationUnitCursor(unit);
    std::vector<CXCursor> unit_cursors;

   parseCursor(translation_unit_cursor, unit_cursors);

    for (const auto &cursor : unit_cursors) {
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_FunctionDecl
            /*&& isFunctionDefinition(cursor)*/) {
            if (current_program->hasFunction(clang_getCString(clang_getCursorSpelling(cursor)))
                || (ignored_functions.count(clang_getCString(clang_getCursorSpelling(cursor))) > 0)) {
                continue;
            }
            CXCursor fun_cursor;
            if (!isFunctionDefinition(cursor)) {
                auto it = std::find_if(unit_cursors.cbegin(), unit_cursors.cend(), [&](CXCursor cur) -> bool {
                    bool is_fun_name_matching = (strcmp(clang_getCString(clang_getCursorSpelling(cursor)), clang_getCString(clang_getCursorSpelling(cur))) == 0);
                    return ((clang_getCursorKind(cur) == CXCursorKind::CXCursor_FunctionDecl) && (is_fun_name_matching) && (isFunctionDefinition(cur)));
                    });
                if (it != unit_cursors.cend()) {
                    fun_cursor = *it;
                } else {
                    std::cout << "function: " << clang_getCString(clang_getCursorSpelling(cursor)) << " has no definition and not added to program\n";
                    continue;
                }
            } else {
                fun_cursor = cursor;
            }
            std::vector<CXCursor> function_cursors;
            function_cursors.push_back(fun_cursor);
            parseCursor(fun_cursor, function_cursors);
            parseFunction(function_cursors);
        }
    }
}

void SourceParser::getFunction(const CXCursor &call_cursor)
{
    auto translation_unit = clang_Cursor_getTranslationUnit(call_cursor);
    auto translation_unit_cursor = clang_getTranslationUnitCursor(translation_unit);
    std::vector<CXCursor> unit_cursors;
    auto fun_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(call_cursor)));

    parseCursor(translation_unit_cursor, unit_cursors);
    for (const auto &cursor : unit_cursors) {
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_FunctionDecl
            && (strcmp(fun_name.c_str(), clang_getCString(clang_getCursorSpelling(cursor))) == 0)) {
            if (current_program->hasFunction(clang_getCString(clang_getCursorSpelling(cursor)))
                || (ignored_functions.count(clang_getCString(clang_getCursorSpelling(cursor))) > 0)) {
                continue;
            }
            CXCursor fun_cursor;
            if (!isFunctionDefinition(cursor)) {
                auto it = std::find_if(unit_cursors.cbegin(), unit_cursors.cend(), [&](CXCursor cur) -> bool {
                    bool is_fun_name_matching = (strcmp(clang_getCString(clang_getCursorSpelling(cursor)), clang_getCString(clang_getCursorSpelling(cur))) == 0);
                    return ((clang_getCursorKind(cur) == CXCursorKind::CXCursor_FunctionDecl) && (is_fun_name_matching) && (isFunctionDefinition(cur)));
                    });
                if (it != unit_cursors.cend()) {
                    fun_cursor = *it;
                } else {
                    std::cout << "function: " << clang_getCString(clang_getCursorSpelling(cursor)) << " has no definition and not added to program\n";
                    continue;
                }
            } else {
                fun_cursor = cursor;
            }
            std::vector<CXCursor> function_cursors;
            function_cursors.push_back(fun_cursor);
            parseCursor(fun_cursor, function_cursors);
            parseFunction(function_cursors);
            break;
        }
    }
}

bool SourceParser::parseFunction(const std::vector<CXCursor> &function_cursors)
{
    if (!isFunDeclEffectOnHeap(function_cursors) && (strcmp(clang_getCString(clang_getCursorSpelling(function_cursors.at(0))), "main") != 0)) {
        //std::cout << "function: " << clang_getCString(clang_getCursorSpelling(function_cursors.at(0))) << " was not effect on heap and not added to program\n";
        ignored_functions.insert(clang_getCString(clang_getCursorSpelling(function_cursors.at(0))));
        return false;
    }

    auto function = std::make_shared<Function>();

    std::vector<CXCursor> body;

    bool body_start{false};
    CXCursor cmpnd_stmt_cursor;
    for (const auto &op : function_cursors) {
        if (body_start) body.push_back(op);
        if (clang_getCursorKind(op) == CXCursorKind::CXCursor_CompoundStmt) {
            cmpnd_stmt_cursor = op;
            body.push_back(op);
            body_start = true;
        }
    }
    function->setBody(body);

    auto return_type = clang_getCursorResultType(function_cursors.at(0));
    int level_indirection{0};
    auto is_const = isTypeConst(return_type);
    auto is_ptr = isCursorTypePointer(return_type, level_indirection);
    auto type_name = getTypeSpelling(return_type);
    function->setReturnType(Type(type_name, is_const, is_ptr, level_indirection));
    
    function->setName(clang_getCString(clang_getCursorSpelling(function_cursors.at(0))));

    auto param_num = clang_Cursor_getNumArguments(function_cursors.at(0));
    for (decltype(param_num) i = 0; i < param_num; ++i) {
        auto param = clang_Cursor_getArgument(function_cursors.at(0), i);
        //int counter = 0;
        //ParseData data;
        //data.current_repr = clang_getCString(clang_getCursorSpelling(param));
        //parseOperator(cmpnd_stmt_cursor, 0, data.current_repr, data, counter);
        //data.is_arg_effect_on_heap
        auto param_type = clang_getCursorType(param);
        int level_indirection{0};
        int value{-1};
        auto is_ptr = isCursorTypePointer(param_type, level_indirection);
        auto is_const = isTypeConst(param_type);
        auto type_name = getTypeSpelling(param_type);
        auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(param)));
        int size = (is_ptr && (level_indirection > 1)) ? 1 : current_program->hasUserType(type_name)
            ? current_program->getUserTypeByName(type_name)->getSize() : 1;
        Type type{type_name, is_const, is_ptr, level_indirection, size};
        Object object;
        CallableObject::Parameter function_param;
        if (is_ptr) {
            object.setType(type);
            if (getParamDefaultValueIfItHave(param, value)) {
                if (type.isPointer()) {
                    object.setAddressVal(Address(value));
                } else if (type.getTypeName() == "int") {
                    object.setIntVal(value);
                } else if (type.getTypeName() == "bool") {
                    object.setBoolVal(value);
                }
            }
            function_param.argument = Variable{name, type};
            function_param.default_val = object;
            function_param.is_used = true;
            function_param.num = i;
            function->addParam(function_param);
        } else {
            function_param.argument = Variable{name, type};
            function_param.default_val = object;
            function_param.is_used = false;
            function_param.num = i;
            function->addParam(function_param);
        }
        /*CXType param_type = clang_getCursorType(param);
        int level_indirection = 0;
        if (isCursorTypePointer(param_type, level_indirection)) {
            int value = -1;
            bool is_ptr = true;
            bool is_const = isTypeConst(param_type);
            std::string type_name = getTypeSpelling(param_type);
            std::string name = clang_getCString(clang_getCursorSpelling(param));
            getParamDefaultValueIfItHave(param, value);
            function->addParam(Variable(name, Type(type_name, is_const, is_ptr, level_indirection), Address(value)));
        }*/
    }


    if (current_program && current_program->addFunction(function)) {
        //std::cout << "function " << function->getName() << " was added to program\n";
    } else {
        //std::cout << "fuctionn " << function->getName() << " was not added to program\n";
    }
    return false;
}

Variable SourceParser::parseParam(CXCursor param)
{
    Variable var;
    Type type;
    auto param_type = clang_getCursorType(param);
    
    int level_indirection{0};
    int value{-1};
    auto is_const = isTypeConst(param_type);
    auto is_ptr = isCursorTypePointer(param_type, level_indirection);
    auto type_name = getTypeSpelling(param_type);
    auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(param)));
    
    var.setName(name);
    type.setTypeName(type_name);
    type.setConstQualified(is_const);
    if (is_ptr) {
        getParamDefaultValueIfItHave(param, value);
        type.setLevelIndirection(level_indirection);
        type.setPointerFlag(is_ptr);
        var.setType(type);
        var.setAddress(Address{value});
    }

    return var;
}

bool SourceParser::isStructDeclEffectOnHeap(const std::vector<CXCursor> &struct_cursors)
{
    for (const auto &cursor : struct_cursors) {
        int level_indirection{0};
        auto cursor_type = clang_getCursorType(cursor);
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_FieldDecl
            && (isCursorTypePointer(cursor_type, level_indirection) || isUserTypeEffectOnHeap(cursor_type))) {
            return true;
        }
    }

    return false;
}

bool SourceParser::isFunDeclEffectOnHeap(const std::vector<CXCursor> &fun_cursors)
{
    bool need_to_analyze{false};
    CXCursor cmpnd_stmt_cursor;
    for (const auto &cur : fun_cursors) {
        if (clang_getCursorKind(cur) == CXCursorKind::CXCursor_CompoundStmt) {
            cmpnd_stmt_cursor = cur;
            break;
        }
    }

    auto return_type = clang_getCursorResultType(fun_cursors.at(0));
    int level_indirection{0};
    if (isCursorTypePointer(return_type, level_indirection)
        || (current_program->hasUserType(clang_getCString(clang_getTypeSpelling(return_type))) && isUserTypeEffectOnHeap(return_type))) {
        return true;
    }

    auto param_num = clang_Cursor_getNumArguments(fun_cursors.at(0));
    for (decltype(param_num) i = 0; i < param_num; ++i) {
        auto param = clang_Cursor_getArgument(fun_cursors.at(0), i);
        auto param_type = clang_getCursorType(param);
        /*ParseData data;
        data.current_repr = clang_getCString(clang_getCursorSpelling(param));
        int counter = 0;
        parseOperator(cmpnd_stmt_cursor, 0, data.current_repr, data, counter);
        data.is_arg_effect_on_heap*/
        if (isCursorTypePointer(param_type, level_indirection)) {
            return true;
        }
    }

    bool body_start{false};
    for (const auto &op : fun_cursors) {
        if (body_start) {
            if ((clang_getCursorKind(op) == CXCursorKind::CXCursor_VarDecl)
                && isCursorTypePointer(clang_getCursorType(op), level_indirection)) {
                return true;
            } else if ((clang_getCursorKind(op) == CXCursorKind::CXCursor_VarDecl)
                && current_program->hasUserType(clang_getCString(clang_getTypeSpelling(clang_getCursorType(op))))
                && isUserTypeEffectOnHeap(clang_getCursorType(op))) {
                return true;
            } else if (clang_getCursorKind(op) == CXCursorKind::CXCursor_CallExpr) {
                if (!current_program->hasFunction(clang_getCString(clang_getCursorSpelling(op)))) {
                    getFunction(op);
                    if (!current_program->hasFunction(clang_getCString(clang_getCursorSpelling(op)))) {
                        continue;
                    }
                }
                return true;
            }
        }
        if (clang_getCursorKind(op) == CXCursorKind::CXCursor_CompoundStmt) body_start = true;
    }

    return false;
}

/*
Выполняется проверка на то, является ли тип курсора пользовательским (уже добавленным в программу)
и оказывает ли он влияние на кучу. Если тип пользовательский и оказывает
влияние на кучу, то тогда возвращается true. Если тип пользовательский, но
не оказывает влияния на кучу, то вернет false. Доп. проверка, на то,
добавлен ли уже рассматриваемый тип в программу не выполняется.
Предполагается, что рассматирваемый пользовательский тип уже был добавлен
в программу. Перед вызовом необходимо вызвать проверить принадлежность
пользовательского типа программе: метод hasUserType.
*/
bool SourceParser::isUserTypeEffectOnHeap(CXType ut_type)
{
    auto type_name = static_cast<std::string>(clang_getCString(clang_getTypeSpelling(ut_type)));
    auto user_type_it = std::find_if(current_program->getUserTypeDeclarations().cbegin(),
        current_program->getUserTypeDeclarations().cend(), [&](const std::pair<std::string, const std::shared_ptr<UserType> > &user_type) {
            return ((type_name == user_type.second->getTypeName()) && !user_type.second->isDummy());
        });
    return (user_type_it != current_program->getUserTypeDeclarations().cend());
}

bool SourceParser::isFunctionDefinition(CXCursor fun_cursor)
{
    auto find_compound_stmt = [](CXCursor cursor, CXCursor parent, CXClientData data) -> CXChildVisitResult {
        auto is_find = static_cast<bool *>(data);
        if (clang_getCursorKind(cursor) == CXCursorKind::CXCursor_CompoundStmt) {
            *is_find = true;
            return CXChildVisit_Break;
        }
        return CXChildVisit_Recurse;
    };

    bool is_definition{false};

    clang_visitChildren(fun_cursor, find_compound_stmt, static_cast<void *>(&is_definition));

    return is_definition;
}

CXTranslationUnit SourceParser::getTranslationUnit(const std::string &file_name,
    int exclude_declaration_from_PCH,
    int display_diagnostics,
    const char *const *command_line_args,
    int num_command_line_args,
    struct CXUnsavedFile *unsaved_files,
    unsigned num_unsaved_files,
    unsigned options)
{
    auto index = clang_createIndex(exclude_declaration_from_PCH, display_diagnostics);

    auto unit = clang_parseTranslationUnit(index,
        file_name.c_str(),
        command_line_args,
        num_command_line_args,
        unsaved_files,
        num_unsaved_files,
        options);

    clang_disposeIndex(index);

    return unit;
}