#include "stdafx.h"
#include "Program.h"

static void printPtrLevelIndirection(const int lvl_num)
{
    for (int i = 0; i < lvl_num; ++i) {
        std::cout << "*";
    }
}

void printUserType(const std::shared_ptr<UserType> ut)
{
    std::cout << "UserType: " << ut->getTypeName();
    if (ut->isDummy()) {
        std::cout << " is dummy\n";
        return;
    }
    std::cout << std::endl;
    std::cout << "Constructors: " << ut->getConstructors().size() << std::endl;
    for (const auto &c : ut->getConstructors()) {
        if (c.isDefault()) {
            std::cout << "\t" << ut->getTypeName() << "()\n";
            continue;
        }
        std::cout << "\t" << ut->getTypeName() << "(";
        for (const auto &p : c.getSignature()) {
            if (!p.is_used) continue;
            if (p.argument.getType().isConst()) std::cout << "const ";
            std::cout << p.argument.getType().getTypeName() << " ";
            printPtrLevelIndirection(p.argument.getType().getLevelIndirection());
            std::cout << p.argument.getName();
            if (p.argument.getAddress().getAddressValue() != -1) std::cout << " = " << p.argument.getAddress().getAddressValue();
            std::cout << " num(" << p.num << ")";
            if (p != c.getSignature().back()) std::cout << ",";
        }
        std::cout << ")\n";
    }
    std::cout << "Pointer fields:\n";
    for (const auto &ptr : ut->getFields()) {
        if (!ptr.getType().isPointer()) continue;
        if (ptr.getType().isConst()) std::cout << "const ";
        std::cout << "\t" << ptr.getType().getTypeName() << " ";
        printPtrLevelIndirection(ptr.getType().getLevelIndirection());
        std::cout << ptr.getName() << "\n";
    }
}

void printUserTypes(const UserTypeDeclarations &ut)
{
    for (const auto &[type_name, user_type_ptr] : ut)
    {
        printUserType(user_type_ptr);
    }
}

void printFunctions(const FunDeclarations &functions)
{
    for (const auto &[name, func] : functions) {
        std::cout << "function: " << name << std::endl;
        std::cout << "\t" << "return type: ";
        if (func->getReturnType().isConst()) std::cout << "const ";
        std::cout << func->getReturnType().getTypeName();
        if (func->getReturnType().isPointer()) {
            std::cout << " ";
            printPtrLevelIndirection(func->getReturnType().getLevelIndirection());
        }
        std::cout << std::endl;
        std::cout << "\t" << "arguments:\n";
        for (const auto &arg : func->getSignature()) {
            if (!arg.is_used) continue;
            std::cout << "\t\t";
            if (arg.argument.getType().isConst()) std::cout << "const ";
            std::cout << arg.argument.getType().getTypeName() << " ";
            printPtrLevelIndirection(arg.argument.getType().getLevelIndirection());
            std::cout << arg.argument.getName();
            if (arg.argument.getAddress().getAddressValue() != -1) std::cout << " = " << arg.argument.getAddress().getAddressValue();
            std::cout << " num(" << arg.num << ")\n";
        }
    }
}

void printFunctionMemoryState(const Function &function)
{
    std::cout << function.getName() << " memory state:\n";
    for (const auto &var : function.getVariables()) {
        std::cout << "var: " << var->getName() << " " << var->getAddress().getAddressValue() << " <-> ";
        auto obj = function.getObjByAddress(var->getAddress());
        if (obj) {
            std::cout << "obj: " << obj->getAddress().getAddressValue() << " " << (obj->getType().isPointer() ? std::to_string(obj->getAddressVal().getAddressValue()) : "") << std::endl;
        } else {
            std::cout << "obj with address " << var->getAddress().getAddressValue() << " is also not exist\n";
        }
        for (const auto &field : obj->getPtrFields()) {
            std::cout << "\t" << field.first.getName() << " " << field.first.getAddress().getAddressValue() << " <-> ";
            if (field.second) {
                std::cout << field.second->getAddressVal().getAddressValue() << std::endl;
            } else {
                std::cout << "nullptr\n";
            }
        }
    }
}

void printHeapState(const Heap &heap)
{
    std::cout << "Heap state:\n";
    for (const auto &obj : heap.getHeap()) {
        std::cout << "obj: " << obj->getType().getTypeName() << " " << obj->getAddress().getAddressValue() << std::endl;
        for (const auto &field : obj->getPtrFields()) {
            std::cout << "\t" << field.first.getName() << " " << field.first.getAddress().getAddressValue() << " <-> "; 
            if (field.second) {
                std::cout << field.second->getAddressVal().getAddressValue() << std::endl;
            } else {
                std::cout << "nullptr\n";
            }
        }
    }
}

void UserType::setSize(const int s)
{
    size = s;
    countTypeSize();
}

void UserType::countTypeSize()
{
    Address address{0};
    for (auto &f : fields) {
        f.setAddress(address);
        address.setAddressValue(address.getAddressValue() + f.getType().getSize());
    }
}

void resetBinaryOpParseDataFlags(ParseData &data)
{
    //data.is_arg_effect_on_heap = false;
    data.is_arg_used = false;
    data.is_field_assigned = false;
    data.is_var_another_ut_field_assigned = false;
    data.is_another_ut_field_from_field_find = false;
    data.is_next_var = false;
    data.is_delete = false;
    data.is_new_base = false;
    data.is_new_base_paren = false;
    data.is_new_base_arr = false;
    data.is_new_ut = false;
    data.is_new_ut_paren = false;
    data.is_new_ut_arr = false;
    data.is_fun = false;
}