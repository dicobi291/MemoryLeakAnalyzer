#pragma once

#include <clang-c/Index.h>
#include "Path.h"

class Type
{
public:
    explicit Type(const std::string &name = "",
        const bool const_ = false,
        const bool is_ptr = false,
        const int lvl_indir = 0,
        const int size_ = 0) : 
        type_name(name),
        is_const(const_),
        is_pointer(is_ptr),
        level_indirection(lvl_indir),
        size(size_){}

    void setTypeName(const std::string &new_name) { type_name = new_name; }
    void setPointerFlag(const bool is_ptr) { is_pointer = is_ptr; }
    void setConstQualified(const bool const_) { is_const = const_; }
    void setLevelIndirection(const int lvl_indir) { level_indirection = lvl_indir; }
    void setSize(const int size_) { size = size_; }

    std::string getTypeName() const { return type_name; }
    bool isPointer() const { return is_pointer; }
    bool isConst() const { return is_const; }
    int getLevelIndirection() const { return level_indirection; }
    int getSize() const { return size; }

    bool operator!=(const Type &t) const {
        return ((type_name != t.type_name) && (is_pointer != t.is_pointer)
            && (level_indirection != t.level_indirection) && (is_const != t.is_const)
            && (size != t.size));
    }

private:
    std::string type_name;
    bool is_const;
    bool is_pointer;
    int level_indirection;
    int size;
};

constexpr int UNINITIALIZED_ADDRESS{-1};

class Address
{
public:
    explicit Address(const int addr = -1) : address_value(addr) {}

    void setAddressValue(const int addr) { address_value = addr; }
    int getAddressValue() const { return address_value; }

    bool operator!=(const Address &addr) const {
        return address_value != addr.address_value;
    }

    bool operator<(const Address &addr) const {
        return address_value < addr.address_value;
    }
private:
    int address_value;
};

enum class ContextType {
    None,
    Program,
    Function,
    Compound,
    Class,
    Constructor,
    Method,
    IfParen,
    IfBody,
    ElseIfParen,
    ElseIfBody,
    Else,
    For,
    While,
    DoWhile,
    BinaryOperator
};

enum class OperatorType {
    None,
    BinaryOperator = 0x1,
    CallExpr = 0x2,
    DeleteExpr = 0x4,
    DeclAssign = 0x8                         
};

enum class CallExprType {
    None,
    NewExprBaseType,
    NewExprBaseTypeParen,
    NewExprBaseTypeArr,
    NewExprUserType,
    NewExprUserTypeParen,
    NewExprUserTypeArr,
    FunctionExpr,
    MethodExpr
};

enum class BinaryOperatorType {
    None,
    Declaration = 0x1,
    Assign = 0x2,
    Comparsion = 0x4,
    Logical = 0x8,
    Arithmetic = 0x10
};

enum class ParseResult {
    None,
    ArgAssignToStructField,
    ArgPtrAssignToStructFieldPtr,
    ArgAssignToFieldStructField,
    ArgAssignToVar,
    ArgPtrAssignToVarPtr,
    VarAssignToArg,
    VarPtrAssignToArgPtr,
    FieldAssignToArgPtr
};

struct ParseData {
    std::string current_repr;
    bool is_arg_effect_on_heap;
    bool is_arg_used;
    bool is_field_assigned;
    bool is_var_another_ut_field_assigned;
    bool is_another_ut_field_from_field_find;
    bool is_next_var;
    bool is_delete;
    bool is_new_base;
    bool is_new_base_paren;
    bool is_new_base_arr;
    bool is_new_ut;
    bool is_new_ut_paren;
    bool is_new_ut_arr;
    bool is_fun;
    ParseData(const std::string cur_rep = "",
        const bool arg_effect_on_heap = false,
        const bool arg_used = false,
        const bool field_assigned = false,
        const bool var_ut_another_field_assigned = false,
        const bool another_ut_field_from_field_find = false,
        const bool next_var = false,
        const bool delete_ = false,
        const bool new_base = false,
        const bool new_base_paren = false,
        const bool new_base_arr = false,
        const bool new_ut = false,
        const bool new_ut_paren = false,
        const bool new_ut_arr = false,
        const bool fun = false) :
        current_repr(cur_rep),
        is_arg_effect_on_heap(arg_effect_on_heap),
        is_arg_used(arg_used),
        is_field_assigned(field_assigned),
        is_var_another_ut_field_assigned(var_ut_another_field_assigned),
        is_another_ut_field_from_field_find(another_ut_field_from_field_find),
        is_next_var(next_var),
        is_delete(delete_),
        is_new_base(new_base),
        is_new_base_paren(new_base_paren),
        is_new_base_arr(new_base_arr),
        is_new_ut(new_ut),
        is_new_ut_paren(new_ut_paren),
        is_new_ut_arr(new_ut_arr),
        is_fun(fun) {}
};

void resetBinaryOpParseDataFlags(ParseData &data);

struct ContextData
{
    std::string current_repr;
    BinaryOperatorType bin_op_type;
    bool in_recurse;
    ContextData(const std::string cur_repr = "",
        const BinaryOperatorType type = BinaryOperatorType::None,
        bool in_rec = false) :
        current_repr(cur_repr),
        bin_op_type(type),
        in_recurse(in_rec) {}
};

class Context
{
public:
    Context(const std::string ctx_name = "",
        const ContextType ctx_tp = ContextType::None,
        Context *prnt = nullptr) :
        context_name(ctx_name),
        parent_context(prnt),
        context_type(ctx_tp) {}

    void setContextName(const std::string &name) { context_name = name; }
    void setContextType(const ContextType ctx_type) { context_type = ctx_type; }
    void setParentContext(const std::shared_ptr<Context> &parent) { parent_context = parent; }

    const std::string &getContextName() const { return context_name; }
    const ContextType &getContextType() const { return context_type; }
    const std::shared_ptr<Context> getParentContext() const { return parent_context; }
private:
    std::string context_name;
    ContextType context_type;
    std::shared_ptr<Context> parent_context;
};

class Variable
{
public:
    Variable(const std::string &nm = "",
        const Type &t = Type(),
        const Address &addr = Address()) :
        name(nm),
        type(t),
        address(addr) {}

    std::string getName() const { return name; }
    Type getType() const { return type; }
    Address getAddress() const { return address; }
    
    void setName(const std::string &new_name) { name = new_name; }
    void setType(const Type &t) { type = t; }
    void setAddress(const Address &addr) { address = addr; }

    bool operator!=(const Variable & v) const {
        return ((name != v.name) && (type != v.type) && (address != v.address));
    }

private:
    std::string name;
    Type type;
    Address address;
};

const std::string HEAP_CONTEXT = "heap";

class Object
{
public:
    using field = std::pair<Variable, std::shared_ptr<Object> >;
    Object() {}
    Object(const Address &addr,
        const Type &t,
        const std::string context = "") :
        address(addr),
        type(t),
        context_name(context){}

    void setAddress(const Address &addr) { address = addr; }
    void setType(const Type &t) { type = t; }
    void addPointerField(const Variable &ptr_field) { pointer_fields.push_back(std::make_pair(ptr_field, std::make_shared<Object>(ptr_field.getAddress(), ptr_field.getType()))); }
    void setPointerFields(const std::vector<field> &ptr_fields) { pointer_fields = ptr_fields; }
    void setIntVal(const int i_val) { int_val = i_val; }
    void setBoolVal(const bool b_val) { bool_val = b_val; }
    void setAddressVal(const Address &addr_val) { address_val = addr_val; }
    void setContextName(const std::string &context) { context_name = context; }

    Address getAddress() const { return address; }
    Type getType() const { return type; }
    int getIntVal() const { return int_val; }
    bool getBoolVal() const { return bool_val; }
    Address getAddressVal() const { return address_val; }
    std::string getContextName() const { return context_name; }
    const std::shared_ptr <Object> getPtrFieldByName(const std::string &field_name) {
        for (const auto &field : pointer_fields) {
            if (field.first.getName() == field_name) {
                return field.second;
            }
        }
        return nullptr;
    }
    const std::vector<field> &getPtrFields() const { return pointer_fields; }

    bool operator!=(const Object &obj) const{
        return ((address != obj.address) && (type != obj.type) && (int_val != obj.int_val)
                && (bool_val != obj.bool_val) && (address_val != obj.address_val) && !compareFieldsEq(obj));
    }

    Object &operator=(const Object &obj) {
        if (this == &obj) {
            return *this;
        }
        address_val = obj.address_val;
        pointer_fields.clear();
        for (const auto &f : obj.pointer_fields) {
            pointer_fields.emplace_back();
            pointer_fields.back().first = f.first;
            *pointer_fields.back().second = *f.second;
        }

        return *this;
    }
private:
    Address address;
    Type type;
    int int_val;
    bool bool_val;
    Address address_val;
    std::string context_name;
    std::vector<field> pointer_fields;

    bool compareFieldsEq(const Object &obj) const {
        if (pointer_fields.size() != obj.pointer_fields.size()) {
            return false;
        }
        for (decltype(pointer_fields.size()) i = 0; i < pointer_fields.size(); ++i) {
            if (pointer_fields.at(i).first != obj.pointer_fields.at(i).first) {
                return false;
            }
        }
        return true;
    }
};

//class Stack
//{
//public:
//    typedef std::vector<Object> StackObjects;
//    Stack() {}
//
//    void addObjToStack(const Object &obj) { stack_objects.push_back(obj); }
//
//private:
//    StackObjects stack_objects;
//};

class CallableObject
{
public:
    struct Parameter {
        Variable argument;
        int num;
        bool is_used;
        Object default_val;
        bool operator!=(const Parameter &p) const {
            return ((argument != p.argument) && (num != p.num)
                && (is_used != p.is_used) && (default_val != p.default_val));
        }
    };
    //т.к. ведется поиск утечки ресурсов, то значение имеют лишь параметры указатели
    //следовательно, достаточно хранить значения только для них, а их значениями
    //являются адреса, представленные в программе целыми числами
    //параметр функции <"имя типа", "имя переменной", значение по умолчанию>

    using Stack = std::vector<std::shared_ptr<Object> >;

    CallableObject() {}
    void setName(const std::string &nm) { name = nm; }
    void addParam(const Parameter &p) { signature.push_back(p); }
    void addVariable(const std::shared_ptr<Variable> var) { variables.push_back(var); }
    void addObjToStack(const std::shared_ptr<Object> &obj) { stack.push_back(obj); }
    void setParam(const int arg_num, const Parameter &p) { signature.at(arg_num) = p; }
    void setParamDefaultValue(const int arg_num, const Object &p) { signature.at(arg_num).default_val = p; }
    void setSignature(const std::vector<Parameter> &s) { signature = s; }
    void setBody(const std::vector<CXCursor> &b) { body = b; }

    std::string getName() const { return name; }
    const Parameter &getParameterByNum(const int arg_num) const { 
        auto argument = std::find_if(signature.cbegin(), signature.cend(), [num = arg_num](const Parameter &param) {
            return (param.num == num);
            });

        return *argument;
    }
    const Parameter &getParameterByName(const std::string &arg_name) const { 
        auto argument = std::find_if(signature.cbegin(), signature.cend(), [name = arg_name](const Parameter &param) {
            return (param.argument.getName() == name);
            });

        return *argument;
    }
    const std::shared_ptr<Variable> getVarByName(const std::string &name) const {
        auto var = std::find_if(variables.cbegin(), variables.cend(), [&](const std::shared_ptr<Variable> &v) {
            return (v->getName() == name);
            });
        return (var != variables.cend()) ? *var : nullptr;
    }
    const std::shared_ptr<Object> getObjByAddress(const Address &addr) const {
        auto object = std::find_if(stack.cbegin(), stack.cend(), [&](const std::shared_ptr<Object> &obj) {
            return (obj->getAddress().getAddressValue() == addr.getAddressValue());
            });
        return (object != stack.cend()) ? *object : nullptr;
    }
    const std::vector<Parameter> getSignature() const { return signature; }
    const std::vector<std::shared_ptr<Variable> > &getVariables() const { return variables; }
    const std::vector<CXCursor> &getBody() const { return body; }

    const Stack &getStack() const { return stack; }
    Variable clearStack() {
        Variable last_variable = variables.empty() ? Variable() : *variables.back();
        stack.clear();
        variables.clear();
        return last_variable;
    }
    CallableObject &operator=(const CallableObject &obj) {
        if (this == &obj) {
            return *this;
        }
        name = obj.name;
        signature = obj.signature;
        body = obj.body;
        stack = obj.stack;
        return *this;
    }

protected:
    std::string name;
    //сигнатура функции; вектор всех его параметров
    std::vector<Parameter> signature;
    //тело функции; вектор всех операторов конструктора
    std::vector<CXCursor> body;
    Stack stack;
    std::vector<std::shared_ptr<Variable> > variables;
};

class Constructor : public CallableObject
{
public:
    //<имя поля, инициализирующие выражение>
    //инициализирующее выражение - это последовательность курсоров, указывающих на
    //вызываемые операторы во время инициализации полей, при использовании списка
    //инициализаторов членов
    using InitializeList = std::vector<std::pair<std::string, std::vector<CXCursor> > >;

    Constructor(bool def = false) : CallableObject(), is_default(def) {}

    void setInitializeList(const InitializeList &init_list) { initialize_list = init_list; }

    bool isDefault() const { return is_default; }
    const InitializeList &getInitializeList() const { return initialize_list; }
private:
    //список инициализаторов членов
    InitializeList initialize_list;
    bool is_default;
};

class ClassMethod : public CallableObject
{
public:
    ClassMethod() : CallableObject() {}

    void setReturnType(const Type &t) { return_type = t; }

    Type getReturnType() const { return return_type; }
private:
    Type return_type;
};

class Function : public CallableObject
{
public:
    Function() : CallableObject() {}

    void setReturnType(const Type &t) { return_type = t; }

    Type getReturnType() const { return return_type; }
private:
    //Критерии необходимости анализа функции.
    //Если функция не оказывает влияния на кучу, а именно:
    //  - ее возвращаемый тип НЕ является указателем или пользовательским
    //    типом(см. критерии влияния на кучу класса UserType);
    //  - ни один из аргументов функции НЕ является указателем или пользовательским типом;
    //  - в теле функции НЕ содержится ни одного объявления указателя или
    //    пользовательского типа и, нету ни одного вызова функции или метода класса
    //    оказывающего влияния на кучу;
    //то такая функция не добавляется в программу.
    Type return_type;
};

class UserType
{
public:
    using field = Variable;

    UserType(const std::string &nm = "",
        const bool dum = false) :
        type_name(nm),
        dummy(dum) {}

    void setTypeName(const std::string &nm) { type_name = nm; }
    void setDummy(const bool dum) { dummy = dum; }
    void addConstructor(const Constructor &c) { constructors.push_back(c); }
    void addField(const field &p) { fields.push_back(p); }
    void setSize(const int s);

    std::string getTypeName() const { return type_name; }
    bool isDummy() const { return dummy; }
    bool hasField(const std::string &field_name) const {
        return std::find_if(fields.cbegin(), fields.cend(), [name = field_name](const field &f) {
            return (name == f.getName());
            }) != fields.end();
    }
    //Constructor getConstructorBySignature(const std::vector<Constructor::param> &s);
    const std::vector<field> &getFields() const { return fields; }
    const std::vector<Constructor> &getConstructors() const { return constructors; }
    const std::vector<Constructor> getConstructorsByNumArgs(const unsigned int num_arg) {
        std::vector<Constructor> res;
        for (const auto &con : constructors) {
            if (con.getSignature().size() == num_arg) {
                res.push_back(con);
            }
        }
        return res;
    }
    int getSize() const { return size; }

private:
    std::string type_name;
    //если пользовательский тип не оказывает влияния на кучу, а именно:
    //  - не имеет полей указателей
    //  - ни один из его методов не оказывает влияния на кучу (см. критерии влияния на кучу для класса Function)
    //то такой тип добавляется для того, чтобы можно было создать его объект,
    //который может быть размещен в куче. Но поскольку его содержимое на кучу никак не
    //повлияет, то полю dummy(фиктивный) присваивается true, сохраняется имя типа,
    //а остальные поля пустые.
    bool dummy;
    std::vector<Constructor> constructors;
    std::vector<field> fields;
    int size;
    
    void countTypeSize();
};

using UserTypeDeclarations = std::map<std::string, std::shared_ptr<UserType> >;
using FunDeclarations = std::map<std::string, std::shared_ptr<Function> >;

class Program
{
public:
    using Stack = std::vector<std::shared_ptr<Object> >;

    explicit Program(const std::string &name = "") : program_name(name) {}

    std::string getProgramName() const { return program_name; }

    void setFunctions(const FunDeclarations &funs) { functions = funs; }
    bool addFunction(const std::shared_ptr<Function> &f) { 
        if (functions.count(f->getName()) == 0) {
            functions.insert(std::make_pair(f->getName(), f));
            return true;
        } else {
            return false;
        }
    }
    FunDeclarations cloneFunctions() {
        FunDeclarations clone_functions;
        for (const auto &[name, fun] : functions) {
            clone_functions.insert(std::make_pair(name, std::make_shared<Function>()));
            *clone_functions.at(name) = *fun;
        }
        return clone_functions;
    }

    void setUserTypes(const UserTypeDeclarations &utd) { user_types = utd; }
    bool addUserType(const std::shared_ptr<UserType> &ut) {
        if (user_types.count(ut->getTypeName()) == 0) {
            user_types.insert(std::make_pair(ut->getTypeName(), ut));
            return true;
        } else {
            return false;
        }
    }
    UserTypeDeclarations cloneUserTypes() {
        UserTypeDeclarations clone_user_types;
        for (const auto &[name, ut] : user_types) {
            clone_user_types.insert(std::make_pair(name, std::make_shared<UserType>()));
            *clone_user_types.at(name) = *ut;
        }
        return clone_user_types;
    }

    const FunDeclarations &getFunDeclarations() const { return functions; }
    const std::shared_ptr<Function> getFunctionByName(const std::string &fun_name) const {
        if (functions.count(fun_name) > 0) {
            return functions.at(fun_name);
        } else {
            std::cout << fun_name << " does not exist in program\n";
            return nullptr;
        }
    }
    const UserTypeDeclarations &getUserTypeDeclarations() const { return user_types; }
    const std::shared_ptr<UserType> getUserTypeByName(const std::string &ut_name) const {
        if (user_types.count(ut_name) > 0) {
            return user_types.at(ut_name);
        } else {
            std::cout << ut_name << " does not exist in program\n";
            return nullptr;
        }
    }
    bool hasUserType(const std::string &ut_name) { return (user_types.count(ut_name) > 0); }
    bool hasFunction(const std::string &fun_name) { return (functions.count(fun_name) > 0); }

    const std::vector<Function> getFunctionsByNumArgs(const unsigned int num_arg) {
        std::vector<Function> res;
        for (const auto &[name, fun] : functions) {
            if (fun->getSignature().size() == num_arg) {
                res.push_back(*fun);
            }
        }
        return res;
    }
    void addObjToStack(const std::shared_ptr<Object> &obj) { stack.push_back(obj); }
    const std::shared_ptr<Object> getObjByAddress(const Address &addr) const {
        auto object = std::find_if(stack.cbegin(), stack.cend(), [&](const std::shared_ptr<Object> &obj) {
            return (obj->getAddress().getAddressValue() == addr.getAddressValue());
            });
        return (object != stack.cend()) ? *object : nullptr;
    }
    void clearStack(const std::string &context) {
        std::vector<std::shared_ptr<Object> > deleted_obj;
        for (const auto &stack_obj : stack) {
            if (stack_obj->getContextName() == context) {
                deleted_obj.push_back(stack_obj);
            }
        }
        for (const auto &obj : deleted_obj) {
            stack.erase(std::find(stack.cbegin(), stack.cend(), obj));
        }
    }
    const Stack &getStack() const { return stack; }

    int getTypeSize(const std::string &type_name) {
        int size{1};
        for (const auto &[name, type] : user_types) {
            if (name == type_name) {
                size = type->getSize();
                break;
            }
        }
        return size;
    }
private:
    std::string program_name;
    UserTypeDeclarations user_types;
    FunDeclarations functions;
    Stack stack;
    //Path paths;
};

class Heap
{
public:
    using ObjectSet = std::vector<std::shared_ptr<Object> >;

    Heap() {}

    void addObject(const std::shared_ptr<Object> obj) { heap.push_back(obj); }
    void setObjectSet(const ObjectSet &h) { heap = h; }

    void deleteObjectByAddress(const Address &addr) {
        auto deleted_obj = std::find_if(heap.begin(), heap.end(), [&](const std::shared_ptr<Object> &obj) {
            return (obj->getAddress().getAddressValue() == addr.getAddressValue());
            });
        if (deleted_obj != heap.end()) {
            heap.erase(deleted_obj);
        } else {
            std::cout << "Object " << addr.getAddressValue() << " was not deleted\n";
        }
    }
    void clear() { heap.clear(); }
    const std::shared_ptr<Object> getObjectByAddress(const Address &addr) {
        auto object = std::find_if(heap.begin(), heap.end(), [&](const std::shared_ptr<Object> &obj) {
            return (obj->getAddress().getAddressValue() == addr.getAddressValue());
            });
        return (object != heap.end()) ? *object : nullptr;
    }
    bool isEmpty() const {
        return heap.empty();
    }

    const ObjectSet &getHeap() const {
        return heap;
    }

private:
    ObjectSet heap;
};