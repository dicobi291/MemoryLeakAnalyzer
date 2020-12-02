#pragma once

#include <clang-c/Index.h>
class Constructor;
class Function;
class Variable;
class UserType;
class Program;
class Context;
struct ParseData;
struct ContextArgData;
enum class OperatorType;
enum class CallExprType;
enum class BinaryOperatorType;
enum class ParseResult;
enum class ContextType;


class SourceParser
{
public:
	SourceParser() : is_current_program_set(false) {}

	std::shared_ptr<Program> parseSingleFileProgram(const std::string &file_name);
    //void parseCursor(CXCursor cursor, std::vector<CXCursor> &cursors);

private:
    std::shared_ptr<Program> current_program;
    std::shared_ptr<UserType> current_user_type;
    std::shared_ptr<Function> current_function;
    bool is_current_program_set;

    std::set<std::string> ignored_functions;

    void getUserTypes(const CXTranslationUnit &unit);
    void parseStructDecl(const std::vector<CXCursor> &struct_decl_cursors);
    bool isStructFieldEffectOnHeap(CXCursor cursor_field, Variable &field, int &field_size);
    Constructor parseStructConstructor(const std::vector<CXCursor> &constructor_decl);
    bool findConstructorDefinition(CXCursor constructor_decl, CXCursor &constructor_def);
    bool isArgInInitList(CXCursor param, const std::vector<std::pair<std::string, std::vector<CXCursor> > > &init_list);
    bool isArgEffectOnHeap(const CXCursor &op_cursor, const int recursion_depth, std::string &arg_name, ParseResult &result, int &op_counter);
    void parseOperator(const CXCursor &bin_op_cursor, const int recursion_depth, const std::string &arg_name, ParseData &res_data, int &op_counter);
    bool isConstructorArgMatchAndUse(CXCursor arg, unsigned int num, const std::vector<Constructor> &constructors);
    bool isFunctionArgMatchAndUse(CXCursor arg, unsigned int num, const std::vector<Function> &functions);
    OperatorType getOperatorType(const CXCursor &op_cursor);
    CallExprType getOperatorCallExprType(const CXCursor &call_expr_cursor);
    BinaryOperatorType getBinaryOperatorType(const CXCursor &bin_op_cursor);
    ContextType getContextType(const CXCursor &cursor);
    //bool isDeclOperatorAssign(const CXCursor &decl_cursor);
    bool getParamDefaultValueIfItHave(CXCursor param, int &value);

    void getFunctions(const CXTranslationUnit &unit);
    void getFunction(const CXCursor &call_cursor);
    bool parseFunction(const std::vector<CXCursor> &function_cursors);
    Variable parseParam(CXCursor param);
    
    bool isStructDeclEffectOnHeap(const std::vector<CXCursor> &struct_cursors);
    bool isFunDeclEffectOnHeap(const std::vector<CXCursor> &fun_cursors);
    bool isUserTypeEffectOnHeap(CXType ut_type);
    bool isFunctionDefinition(CXCursor fun_cursor);
    
    //bool isCursorTypePointer(CXType type, int &level_indirection);
    //bool isInternalType(const CXType &type);
    //bool isTypeConst(CXType type);
    //std::string getTypeSpelling(CXType type);

    //void tokenize(CXCursor cursor, CXTranslationUnit &unit, CXToken **tokens, unsigned int &num_tokens);
    CXTranslationUnit getTranslationUnit(const std::string &file_name, 
        int exclude_declaration_from_PCH = 1,
        int display_diagnostics = 1,
        const char *const *command_line_args = nullptr,
        int num_command_line_args = 0,
        struct CXUnsavedFile *unsaved_files = nullptr,
        unsigned num_unsaved_files = 0,
        unsigned options = CXTranslationUnit_None);
};

