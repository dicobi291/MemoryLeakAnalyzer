#include "stdafx.h"
#include "Path.h"
#include "Program.h"

void parseCursor(const CXCursor &cursor, std::vector<CXCursor> &cursors);

bool isSectionEqual(const PathSection &sec_1, const PathSection &sec_2)
{
	if (sec_1.operators.size() != sec_2.operators.size()) {
		return false;
	}
	for (size_t i = 0; i < sec_1.operators.size(); i++) {
		if (!clang_equalCursors(sec_1.operators.at(i), sec_2.operators.at(i))) {
			return false;
		}
	}
	return true;
}

Paths PathFormer::formPathsForProgram(const std::shared_ptr<Program> program)
{
	current_program = program;

	std::shared_ptr<Function> main = current_program->getFunctionByName("main");

	//Paths paths;
	Paths paths = formPaths(main->getBody().at(0), main->getName());
	//PathsId paths_id = formPaths_(main->getBody().at(0), main->getName());

	//printPaths(paths);

	current_program.reset();
	
	return paths;
}

/*
Множество всевозможных путей выполнения программы представляет собой вектор
последовательностей отрезков пути (PathSection). Каждый такой отрезок содержит
в себе некоторую последовательность операторов, представленных курсорами,
имя контекста(ф-ции), в котором существует этот отрезок и флаг is_end, показывающий
является ли данный отрезок последним для его контекста, т.е. есть ли там оператор
return или нет.
Формирование множества всех путей выполнения работает по следующему принципу. Обход программы
начинается с ф-ции main, и переходит во все вызовы ф-ций, которые есть в ней и в других ф-циях.
Если встречается оператор if, то это значит, что происходит ветвление, т.е. все пути, что пришли
в точку, где встретился оператор if могут быть выполнены несколькими вариантами, в зависимости
от кол-ва веток оператора if. В этом случае сначала подсчитывается кол-во веток (else-if и, если
есть, else), сохраняются их шапки(проверочные условия) и оператор {, с которого начинается выполнение
блока следующего за if. Затем для всех путей создается столько копий, чтобы их кол-во совпало
с количеством веток if. Это делается для всех путей, кроме тех, что в текущем контексте были
заврешены оператором return. Затем эти копии помещаются в группы, каждая из которых соответствует
отдельной ветке if. После чего во все эти группы путей добавляются шапки операторов if, т.к. перед тем
как будет выполнено тело if, должны быть выполнены проверочные условия. Копирование происходит
по следующему принципу: первая шапка добавляется во все группы, т.к. в какую бы ветку не перешел
поток выполнения программы, первая ветка будет выполнена всегда, вторая шапка во все группы начиная
со второй и т.д. Затем метод уходит в рекурсию для каждого блока. Т.к. внутри блока if путь тоже может
ветвится, то каждый путь, находящийся в группе снова копируется такое кол-во раз, какое нужно для того,
чтобы это было равно кол-ву полученных путей. Копии снова группируются, и в конец всех путей каждой группы
добавляется соотвсетствующий путь. Затем эта процедура повторяется для всех остальных групп. В конце
все пути всех групп помещаются в один вектор, к ним присоединяются "завершенные" пути, отделенные в начале
процедуры и продолжается формирование путей просмотром других операторов.
Для вызовов ф-ций, все происходит немного иначе. Шапка функции, т.е. ее вызов также добавляется во все пути.
Затем, для аргументов функции, рассматривается вариант, вызова на их месте других функций, что может дополнительно
ветвить пути. Поэтому сохраняется имя каждого вызова и соответствующий этому вызову оператор {.
Причем, эти пары сохраняются в стек, т.е. первая сохраненная пара будет обработана последней.
Следующим этапом является рекурсивный вызов ф-ции формирования путей для очередной ф-ции. Результатом
будет вектор путей, поэтому нужно размножить пришедшие в эту точку пути по аналогии с оператором if.
Потом берется следующая ф-ция и выполняется формирование путей для нее, и пути(размноженные на предыдущей
итерации) снова множатся. Так происходит, пока не будут обработаны все вызовы.
Для циклов (while и for), все выполняется проще. Сначала в конец всех путей добавляется шапка цикла, затем тело
цикла обрабатывается и пришедшие в точку выполнения цикла пути множатся в соответствии с результатом.
Опертор return просто добавляется во все пути, которые еще не завершились.
*/
Paths PathFormer::formPaths(const CXCursor &statement, const std::string &context_name)
{
	Paths paths;

	std::vector<CXCursor> stmt_cursors;
	parseCursor(statement, stmt_cursors);

	PathSection path_section;
	paths.emplace_back();
	paths.at(0).push_back(path_section);
	paths.at(0).back().context_name = context_name;

	for (decltype(stmt_cursors.size()) i = 0; i < stmt_cursors.size(); ++i) {
		auto kind = clang_getCursorKind(stmt_cursors.at(i));
		if (kind == CXCursorKind::CXCursor_IfStmt) {
			size_t offset{0};
			std::vector<CXCursor> if_cursors;
			parseCursor(stmt_cursors.at(i), if_cursors);
			offset = if_cursors.size();

			auto if_conditions = getIfConditions(stmt_cursors.at(i));
			
			Paths paths_for_copy;
			Paths ended_paths;
			filterPaths(paths, ended_paths, paths_for_copy, context_name);

			std::vector<BranchGroup> branch_groups;
			//Копирование путей
			for (decltype(if_conditions.branch_numbers) i = 0; i < if_conditions.branch_numbers; ++i) {
				branch_groups.emplace_back();
				branch_groups.back().group_id = i;
				branch_groups.back().paths = paths_for_copy;
			}
			
			//Добавление в конец каждого пути, принадлежащего группе соотвествующей i-ому оператору if, операторов находящихся в проверочном условии if,
			//при чем операторы в самом первом if добавляются во все пути, т.к. они выполняются для всех путей, даже если некоторые из них не войдут в эту ветку.
			//Все остальные операторы проверочных условий добавляются во все пути для групп, начиная с соответствующей номеру оператора else-if.
			for (decltype(if_conditions.branch_numbers)i = 0; i < if_conditions.branch_numbers; ++i) {
				for (decltype(branch_groups.size()) j = i; j < branch_groups.size(); ++j) {
					for (auto &path : branch_groups.at(j).paths) {
						path.back().operators.insert(path.back().operators.end(), if_conditions.conditions.at(i).begin(), if_conditions.conditions.at(i).end());
					}
				}
			}

			Paths finally_paths;
			if ((if_conditions.branch_numbers == 1)
				|| ((if_conditions.conditions.size() == if_conditions.branch_numbers) && (if_conditions.conditions.back().size() != 0))) {
				finally_paths = paths_for_copy;
				for (const auto &condition : if_conditions.conditions) {
					for (auto &path : finally_paths) {
						path.back().operators.insert(path.back().operators.end(), condition.begin(), condition.end());
					}
				}
			}
			for (const auto &br_group : branch_groups) {
				auto added_paths = formPaths(if_conditions.cmpnd_stmts.at(br_group.group_id), context_name);
				auto res_paths_numbers = (br_group.paths.size() * added_paths.size());
				std::vector<BranchGroup> res_branches;
				for (decltype(res_paths_numbers) i = 0; i < res_paths_numbers; ) {
					res_branches.emplace_back();
					res_branches.back() = br_group;
					i += br_group.paths.size();
				}
				for (decltype(added_paths.size()) i = 0; i < added_paths.size(); ++i) {
					for (auto &path : res_branches.at(i).paths) {
						path.insert(path.end(), added_paths.at(i).begin(), added_paths.at(i).end());
					}
				}
				for (const auto &res_branch : res_branches) {
					for (const auto &res_path : res_branch.paths) {
						finally_paths.push_back(res_path);
					}
				}
			}
			paths.clear();
			paths = ended_paths;
			paths.insert(paths.end(), finally_paths.begin(), finally_paths.end());

			if ((i + offset + 1) != stmt_cursors.size()) {
				for (auto &path : paths) {
					if (!path.back().is_end && (path.back().context_name == context_name)
						|| (path.back().context_name != context_name)) {
						path.emplace_back();
						path.back().context_name = context_name;
					}
				}
			}
			i += offset;
			continue;
		}
		if (kind == CXCursorKind::CXCursor_CallExpr && !current_program->hasUserType(clang_getCString(clang_getCursorSpelling(stmt_cursors.at(i))))) {
			auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(stmt_cursors.at(i))));
			
			size_t offset{0};
			std::vector<CXCursor> call_cursors;
			parseCursor(stmt_cursors.at(i), call_cursors);
			offset = call_cursors.size();
			call_cursors.insert(call_cursors.begin(), stmt_cursors.at(i));
			
			if (!current_program->hasFunction(name)) {
				i += offset;
				continue;
			}

			Paths paths_for_copy;
			Paths ended_paths;
			filterPaths(paths, ended_paths, paths_for_copy, context_name);

			auto current_point_paths = paths_for_copy;
			for (auto &path : current_point_paths) {
				path.back().operators.insert(path.back().operators.end(), call_cursors.begin(), call_cursors.end());
			}

			auto call_sequence = getCallSequence(stmt_cursors.at(i));
			while (!call_sequence.empty()) {
				auto current_call = call_sequence.top();
				auto added_paths = formPaths(current_call.second, current_call.first);
				std::vector<BranchGroup> new_branches;
				for (decltype(added_paths.size()) i = 0; i < added_paths.size(); ++i) {
					new_branches.emplace_back();
					new_branches.back().paths = current_point_paths;
					new_branches.back().group_id = i;
				}
				for (decltype(new_branches.size()) i = 0; i < new_branches.size(); ++i) {
					for (auto &path : new_branches.at(i).paths) {
						path.insert(path.end(), added_paths.at(i).begin(), added_paths.at(i).end());
					}
				}
				current_point_paths.clear();
				for (const auto &new_branch : new_branches) {
					for (const auto &path : new_branch.paths) {
						current_point_paths.push_back(path);
					}
				}
				call_sequence.pop();
			}

			paths.clear();
			paths = ended_paths;
			paths.insert(paths.end(), current_point_paths.begin(), current_point_paths.end());
			if ((i + offset + 1) != stmt_cursors.size()) {
				for (auto &path : paths) {
					if (!path.back().is_end && (path.back().context_name == context_name)
						|| (path.back().context_name != context_name)) {
						path.emplace_back();
						path.back().context_name = context_name;
					}
				}
			}
			i += offset;
			continue;
		}
		if ((kind == CXCursorKind::CXCursor_ForStmt)
			|| (kind == CXCursorKind::CXCursor_WhileStmt)) {
			size_t offset{0};
			std::vector<CXCursor> circle_cursors;
			parseCursor(stmt_cursors.at(i), circle_cursors);
			offset = circle_cursors.size();

			auto circle_head = getCircleCmpndStmt(stmt_cursors.at(i));

			Paths paths_for_copy;
			Paths ended_paths;
			filterPaths(paths, ended_paths, paths_for_copy, context_name);
			for (auto &path : paths_for_copy) {
				path.back().operators.insert(path.back().operators.end(), circle_head.head.begin(), circle_head.head.end());
			}

			auto added_paths = formPaths(circle_head.cmpnd_stmt, context_name);
			auto new_path_size = added_paths.size();
			std::vector<BranchGroup> new_branches;
			for (decltype(added_paths.size()) i = 0; i < added_paths.size(); ++i) {
				new_branches.emplace_back();
				new_branches.back().group_id = i;
				new_branches.back().paths = paths_for_copy;
			}
			for (decltype(new_branches.size()) i = 0; i < new_branches.size(); ++i) {
				for (auto &path : new_branches.at(i).paths) {
					path.insert(path.end(), added_paths.at(i).begin(), added_paths.at(i).end());
				}
			}

			paths.clear();
			paths = ended_paths;
			for (const auto &new_branch : new_branches) {
				paths.insert(paths.end(), new_branch.paths.begin(), new_branch.paths.end());
			}
			if ((i + offset + 1) != stmt_cursors.size()) {
				for (auto &path : paths) {
					if (!path.back().is_end && (path.back().context_name == context_name)
						|| (path.back().context_name != context_name)) {
						path.emplace_back();
						path.back().context_name = context_name;
					}
				}
			}
			i += offset;
			continue;
		}
		if (kind == CXCursorKind::CXCursor_ReturnStmt) {
			size_t offset{0};
			std::vector<CXCursor> return_cursors;
			return_cursors.push_back(stmt_cursors.at(i));
			parseCursor(stmt_cursors.at(i), return_cursors);
			offset = return_cursors.size();

			Paths paths_for_op_added;
			Paths ended_paths;
			filterPaths(paths, ended_paths, paths_for_op_added, context_name);

			for (auto &path : paths_for_op_added) {
				path.back().operators.insert(path.back().operators.end(), return_cursors.begin(), return_cursors.end());
				path.back().is_end = true;
			}

			paths.clear();
			paths = ended_paths;
			paths.insert(paths.end(), paths_for_op_added.begin(), paths_for_op_added.end());

			i += offset;
			continue;
		}
		for (auto &path : paths) {
			if (path.back().is_end && (path.back().context_name == context_name)) {
				continue;
			} else {
				path.back().operators.push_back(stmt_cursors.at(i));
			}
		}
	}

	return paths;
}


PathsId PathFormer::formPaths_(const CXCursor &statement, const std::string &context_name)
{
	PathsId paths;

	std::vector<CXCursor> stmt_cursors;
	parseCursor(statement, stmt_cursors);

	PathSection path_section;
	auto current_section_id = path_section.id = getPathSectionId();
	path_section.context_name = context_name;
	path_sections.insert(std::make_pair(path_section.id, path_section));
	paths.emplace_back();
	paths.at(0).push_back(path_section.id);

	for (decltype(stmt_cursors.size()) i = 0; i < stmt_cursors.size(); ++i) {
		auto kind = clang_getCursorKind(stmt_cursors.at(i));
		if (kind == CXCursorKind::CXCursor_IfStmt) {
			size_t offset{0};
			std::vector<CXCursor> if_cursors;
			parseCursor(stmt_cursors.at(i), if_cursors);
			offset = if_cursors.size();

			auto if_conditions = getIfConditions(stmt_cursors.at(i));

			PathsId paths_for_copy;
			PathsId ended_paths;
			filterPaths_(paths, ended_paths, paths_for_copy, context_name);

			std::vector<BranchGroup_> branch_groups;
			//Копирование путей
			for (decltype(if_conditions.branch_numbers) i = 0; i < if_conditions.branch_numbers; ++i) {
				branch_groups.emplace_back();
				branch_groups.back().group_id = i;
				branch_groups.back().last_sections_copy = copyLastSections(paths_for_copy);
				branch_groups.back().paths = paths_for_copy;
			}

			path_sections.erase(branch_groups.at(0).last_sections_copy.at(0).id);

			//Добавление в конец каждого пути, принадлежащего группе соотвествующей i-ому оператору if, операторов находящихся в проверочном условии if,
			//при чем операторы в самом первом if добавляются во все пути, т.к. они выполняются для всех путей, даже если некоторые из них не войдут в эту ветку.
			//Все остальные операторы проверочных условий добавляются во все пути для групп, начиная с соответствующей номеру оператора else-if.
			for (decltype(if_conditions.branch_numbers) i = 0; i < if_conditions.branch_numbers; ++i) {
				for (decltype(branch_groups.size()) j = i; j < branch_groups.size(); ++j) {
					for (auto &sec : branch_groups.at(j).last_sections_copy) {
						sec.operators.insert(sec.operators.end(), if_conditions.conditions.at(i).begin(), if_conditions.conditions.at(i).end());
					}
				}
			}

			for (decltype(branch_groups.size()) i = 0; i < branch_groups.size(); ++i) {
				for (decltype(branch_groups.at(i).last_sections_copy.size()) j = 0; j < branch_groups.at(i).last_sections_copy.size(); ++j) {
					int id{0};
					if (isSectionExist(branch_groups.at(i).last_sections_copy.at(j), id)) {
						branch_groups.at(i).paths.at(j).back() = id;
					} else {
						branch_groups.at(i).last_sections_copy.at(j).id = getPathSectionId();
						branch_groups.at(i).paths.at(j).back() = branch_groups.at(i).last_sections_copy.at(j).id;
						path_sections.insert(std::make_pair(branch_groups.at(i).last_sections_copy.at(j).id, branch_groups.at(i).last_sections_copy.at(j)));
					}
				}
			}

			PathsId finally_paths;
			if ((if_conditions.branch_numbers == 1)
				|| ((if_conditions.conditions.size() == if_conditions.branch_numbers) && (if_conditions.conditions.back().size() != 0))) {
				finally_paths = branch_groups.back().paths;
			}
			for (const auto &br_group : branch_groups) {
				auto added_paths = formPaths_(if_conditions.cmpnd_stmts.at(br_group.group_id), context_name);
				auto res_paths_numbers = (br_group.paths.size() * added_paths.size());
				
				clearExistingSections(added_paths);

				std::vector<BranchGroup_> res_branches;
				for (decltype(res_paths_numbers) i = 0; i < res_paths_numbers; ) {
					res_branches.emplace_back();
					res_branches.back() = br_group;
					i += br_group.paths.size();
				}
				for (decltype(added_paths.size()) i = 0; i < added_paths.size(); ++i) {
					for (auto &path : res_branches.at(i).paths) {
						path.insert(path.end(), added_paths.at(i).begin(), added_paths.at(i).end());
					}
				}
				for (const auto &res_branch : res_branches) {
					for (const auto &res_path : res_branch.paths) {
						finally_paths.push_back(res_path);
					}
				}
			}
			paths.clear();
			paths = ended_paths;
			paths.insert(paths.end(), finally_paths.begin(), finally_paths.end());

			if ((i + offset + 1) != stmt_cursors.size()) {
				PathSection next_section{getPathSectionId(), context_name};
				current_section_id = next_section.id;
				path_sections.insert(std::make_pair(next_section.id, next_section));
				for (auto &path : paths) {
					if ((!path_sections.at(path.back()).is_end && (path_sections.at(path.back()).context_name == context_name))
						|| (path_sections.at(path.back()).context_name != context_name))
					{
						path.push_back(path_sections.rbegin()->first);
						//path.emplace_back();
						//path.back().context_name = context_name;
					}
				}
			}
			i += offset;
			continue;
		}
		if (kind == CXCursorKind::CXCursor_CallExpr && !current_program->hasUserType(clang_getCString(clang_getCursorSpelling(stmt_cursors.at(i))))) {
			auto name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(stmt_cursors.at(i))));

			size_t offset{0};
			std::vector<CXCursor> call_cursors;
			parseCursor(stmt_cursors.at(i), call_cursors);
			offset = call_cursors.size();
			call_cursors.insert(call_cursors.begin(), stmt_cursors.at(i));

			if (!current_program->hasFunction(name)) {
				i += offset;
				continue;
			}

			PathsId paths_for_copy;
			PathsId ended_paths;
			filterPaths_(paths, ended_paths, paths_for_copy, context_name);

			PathsId current_point_paths{paths_for_copy};
			path_sections.at(current_section_id).operators.insert(path_sections.at(current_section_id).operators.end(), call_cursors.begin(), call_cursors.end());

			auto call_sequence = getCallSequence(stmt_cursors.at(i));
			while (!call_sequence.empty()) {
				auto current_call = call_sequence.top();
				auto added_paths = formPaths_(current_call.second, current_call.first);
				
				clearExistingSections(added_paths);

				std::vector<BranchGroup_> new_branches;
				for (decltype(added_paths.size()) i = 0; i < added_paths.size(); ++i) {
					new_branches.emplace_back();
					new_branches.back().paths = current_point_paths;
					new_branches.back().group_id = i;
				}
				for (decltype(new_branches.size()) i = 0; i < new_branches.size(); ++i) {
					for (auto &path : new_branches.at(i).paths) {
						path.insert(path.end(), added_paths.at(i).begin(), added_paths.at(i).end());
					}
				}
				current_point_paths.clear();
				for (const auto &new_branch : new_branches) {
					for (const auto &path : new_branch.paths) {
						current_point_paths.push_back(path);
					}
				}
				call_sequence.pop();
			}

			paths.clear();
			paths = ended_paths;
			paths.insert(paths.end(), current_point_paths.begin(), current_point_paths.end());
			if ((i + offset + 1) != stmt_cursors.size()) {
				PathSection next_section{getPathSectionId(), context_name};
				current_section_id = next_section.id;
				path_sections.insert(std::make_pair(next_section.id, next_section));
				for (auto &path : paths) {
					if ((!path_sections.at(path.back()).is_end && (path_sections.at(path.back()).context_name == context_name))
						|| (path_sections.at(path.back()).context_name != context_name))
					{
						path.push_back(path_sections.rbegin()->first);
						//path.emplace_back();
						//path.back().context_name = context_name;
					}
				}
			}
			i += offset;
			continue;
		}
		if ((kind == CXCursorKind::CXCursor_ForStmt)
			|| (kind == CXCursorKind::CXCursor_WhileStmt)) {
			size_t offset{0};
			std::vector<CXCursor> circle_cursors;
			parseCursor(stmt_cursors.at(i), circle_cursors);
			offset = circle_cursors.size();

			auto circle_head = getCircleCmpndStmt(stmt_cursors.at(i));

			PathsId paths_for_copy;
			PathsId ended_paths;
			filterPaths_(paths, ended_paths, paths_for_copy, context_name);
			
			path_sections.at(current_section_id).operators.insert(path_sections.at(current_section_id).operators.end(), circle_head.head.begin(), circle_head.head.end());

			auto added_paths = formPaths_(circle_head.cmpnd_stmt, context_name);
			
			clearExistingSections(added_paths);

			std::vector<BranchGroup_> new_branches;
			for (decltype(added_paths.size()) i = 0; i < added_paths.size(); ++i) {
				new_branches.emplace_back();
				new_branches.back().group_id = i;
				new_branches.back().paths = paths_for_copy;
			}
			for (decltype(new_branches.size()) i = 0; i < new_branches.size(); ++i) {
				for (auto &path : new_branches.at(i).paths) {
					path.insert(path.end(), added_paths.at(i).begin(), added_paths.at(i).end());
				}
			}

			paths.clear();
			paths = ended_paths;
			for (const auto &new_branch : new_branches) {
				paths.insert(paths.end(), new_branch.paths.begin(), new_branch.paths.end());
			}
			if ((i + offset + 1) != stmt_cursors.size()) {
				PathSection next_section(getPathSectionId(), context_name);
				current_section_id = next_section.id;
				path_sections.insert(std::make_pair(next_section.id, next_section));
				for (auto &path : paths) {
					if ((!path_sections.at(path.back()).is_end && (path_sections.at(path.back()).context_name == context_name))
						|| (path_sections.at(path.back()).context_name != context_name))
					{
						path.push_back(path_sections.rbegin()->first);
						//path.emplace_back();
						//path.back().context_name = context_name;
					}
				}
			}
			i += offset;
			continue;
		}
		if (kind == CXCursorKind::CXCursor_ReturnStmt) {
			size_t offset{0};
			std::vector<CXCursor> return_cursors;
			return_cursors.push_back(stmt_cursors.at(i));
			parseCursor(stmt_cursors.at(i), return_cursors);
			offset = return_cursors.size();

			path_sections.at(current_section_id).operators.insert(path_sections.at(current_section_id).operators.end(), return_cursors.begin(), return_cursors.end());
			path_sections.at(current_section_id).is_end = true;

			i += offset;
			continue;
		}
		/*for (auto &path : paths) {
			if (path_sections.at(path.back()).is_end && (path_sections.at(path.back()).context_name == context_name)) {
				continue;
			} else {
				path_sections.at(path.back()).operators.push_back(stmt_cursors.at(i));
			}
		}*/
		path_sections.at(current_section_id).operators.push_back(stmt_cursors.at(i));
	}

	return paths;
}

int PathFormer::getPathSectionId()
{
	if (path_sections.empty()) {
		return 1;
	} else {
		return ((*path_sections.rbegin()).first + 1);
	}
}

void PathFormer::clearExistingSections(PathsId &paths_id)
{
	std::set<int> delete_id;
	for (auto &path : paths_id) {
		for (auto &id : path) {
			for (const auto &[id_sec, sec] : path_sections) {
				if ((id != id_sec) && (isSectionEqual(path_sections.at(id), sec))) {
					int copy_id = id;
					delete_id.insert(copy_id);
					id = id_sec;
					break;
				}
			}
		}
	}
	for (const auto &id : delete_id) {
		path_sections.erase(id);
	}
}

std::vector<PathSection> PathFormer::copyLastSections(const PathsId &paths_id)
{
	std::vector<PathSection> sections_copy;
	for (const auto &path : paths_id) {
		if (!path_sections.at(path.back()).is_end) {
			sections_copy.push_back(path_sections.at(path.back()));
		}
	}

	return sections_copy;
}

bool PathFormer::isSectionExist(const PathSection &section, int &sec_id)
{
	for (const auto &[id, sec] : path_sections) {
		if (isSectionEqual(sec, section)) {
			sec_id = id;
			return true;
		}
	}
	return false;
}

IfCondition PathFormer::getIfConditions(const CXCursor &if_stmt)
{
	IfCondition if_conditions;
	if_conditions.branch_numbers = 0;

	std::vector<CXCursor> if_stmts;
	if_stmts.push_back(if_stmt);
	parseCursor(if_stmt, if_stmts);

	if_conditions.conditions.push_back(std::vector<CXCursor>());
	for (decltype(if_stmts.size()) i = 0; i < if_stmts.size(); ++i) {
		if (clang_getCursorKind(if_stmts.at(i)) == CXCursorKind::CXCursor_CompoundStmt) {
			if_conditions.cmpnd_stmts.push_back(if_stmts.at(i));
			++if_conditions.branch_numbers;
			std::vector<CXCursor> cmpnd_cursors;
			parseCursor(if_stmts.at(i), cmpnd_cursors);
			i += cmpnd_cursors.size();
			if ((i + 1) < if_stmts.size()) {
				if_conditions.conditions.push_back(std::vector<CXCursor>());
			}
			continue;
		}
		if_conditions.conditions.at(if_conditions.branch_numbers).push_back(if_stmts.at(i));
	}

	return if_conditions;
}

CallSequence PathFormer::getCallSequence(const CXCursor &call_expr)
{
	CallSequence call_sequence;

	std::vector<CXCursor> call_exprs;
	call_exprs.push_back(call_expr);
	parseCursor(call_expr, call_exprs);

	for (decltype(call_exprs.size()) i = 0; i < call_exprs.size(); ++i) {
		if (clang_getCursorKind(call_exprs.at(i)) == CXCursorKind::CXCursor_CallExpr) {
			auto call_name = static_cast<std::string>(clang_getCString(clang_getCursorSpelling(call_exprs.at(i))));
			if (current_program->hasFunction(call_name)) {
				call_sequence.push(std::make_pair(call_name, current_program->getFunctionByName(call_name)->getBody().front()));
			}
		}
	}

	return call_sequence;
}

CircleHead PathFormer::getCircleCmpndStmt(const CXCursor &circle_stmt)
{
	CircleHead circle_head;
	circle_head.head.push_back(circle_stmt);

	std::vector<CXCursor> circle_stmts;
	parseCursor(circle_stmt, circle_stmts);

	for (const auto &stmt : circle_stmts) {
		if (clang_getCursorKind(stmt) == CXCursorKind::CXCursor_CompoundStmt) {
			circle_head.cmpnd_stmt = stmt;
			break;
		}
		circle_head.head.push_back(stmt);
	}

	return circle_head;
}

/* 
Функция отделяет те пути, которые в заданном контексте уже завершились, т.е. в них уже был встречен оператор return.
Все остальные пути помещаются в paths_for_copy. Для них возможно дальнейшее ветвление.
*/
void PathFormer::filterPaths(const Paths &orig_paths, Paths &ended_paths, Paths &paths_for_copy, const std::string &context_name)
{
	for (const auto &path : orig_paths) {
		if (path.back().is_end && (path.back().context_name == context_name)) {
			ended_paths.push_back(path);
		} else {
			paths_for_copy.push_back(path);
		}
	}
}

void PathFormer::filterPaths_(const PathsId &orig_paths, PathsId &ended_paths, PathsId &paths_for_copy, const std::string &context_name)
{
	for (const auto &path : orig_paths) {
		if (path_sections.at(path.back()).is_end && (path_sections.at(path.back()).context_name == context_name)) {
			ended_paths.push_back(path);
		} else {
			paths_for_copy.push_back(path);
		}
	}
}

void PathFormer::printPaths(const Paths &paths)
{
	int paths_count{1};
	for (const auto &path : paths) {
		std::cout << "Path " << paths_count++ << ":\n";
		for (const auto &sec : path) {
			for (const auto &op : sec.operators) {
				auto kind = clang_getCursorKind(op);
				if (kind == CXCursorKind::CXCursor_IfStmt) std::cout << "if ";
				if (kind == CXCursorKind::CXCursor_ForStmt) std::cout << "for ";
				if (kind == CXCursorKind::CXCursor_WhileStmt) std::cout << "while ";
				if (clang_isStatement(kind) || (kind == CXCursorKind::CXCursor_BinaryOperator)
					|| (kind == CXCursorKind::CXCursor_UnaryOperator)) {
					if (kind == CXCursorKind::CXCursor_IfStmt) continue;
					if ((kind == CXCursorKind::CXCursor_ForStmt)
						|| (kind == CXCursorKind::CXCursor_WhileStmt)) continue;
					auto unit = clang_Cursor_getTranslationUnit(op);
					auto src_range = clang_getCursorExtent(op);
					CXToken *tokens{nullptr};
					unsigned int num_tokens{0};
					clang_tokenize(unit, src_range, &tokens, &num_tokens);
					for (decltype(num_tokens) i = 0; i < num_tokens; ++i) {
						std::cout << clang_getCString(clang_getTokenSpelling(unit, tokens[i])) << " ";
					}
					std::cout << std::endl;
					clang_disposeTokens(unit, tokens, num_tokens);
				}
			}
		}
	}
}