#pragma once
#include <clang-c/Index.h>
class Program;

class PathSectorId
{
public:
	PathSectorId(const int i = 0) : id(i) {}

	void setId(const int new_id) { id = new_id; }

	int getId() const { return id; }
	
	bool operator<(const PathSectorId &ps) const {
		return id < ps.id;
	}
private:
	int id;
};

struct PathSection
{
	int id;
	bool is_end;
	std::string context_name;
	std::vector<CXCursor> operators;
	PathSection(const int id_ = 0, const std::string &context = "") : id(id_), context_name(context), is_end(false) {}
};

using Paths = std::vector<std::vector<PathSection> >;
using PathsId = std::vector<std::vector<int> >;

struct IfCondition
{
	int branch_numbers;
	std::vector<CXCursor> cmpnd_stmts;
	std::vector<std::vector<CXCursor> > conditions;
};

struct BranchGroup
{
	int group_id;
	Paths paths;
};

struct BranchGroup_
{
	int group_id;
	std::vector<PathSection> last_sections_copy;
	PathsId paths;
};

struct CircleHead
{
	CXCursor cmpnd_stmt;
	std::vector<CXCursor> head;
};

using CallSequence = std::stack<std::pair<std::string, CXCursor> >;

class Path
{
public:
	Path() {}
	
	void insertPathSector(const std::shared_ptr<PathSection> path_sec) { path_sectors.insert(std::make_pair(path_sec->id, path_sec)); }

	const std::map<PathSectorId, std::shared_ptr<PathSection>> getPathSectors() const { return path_sectors; }
private:
	std::map<PathSectorId, std::shared_ptr<PathSection> > path_sectors;
};

class PathFormer
{
public:
	PathFormer() {}

	Paths formPathsForProgram(const std::shared_ptr<Program> prog);

	const std::map<int, PathSection> &getPathSections() { return path_sections; }

private:
	Paths formPaths(const CXCursor &statement, const std::string &context_name);
	PathsId formPaths_(const CXCursor &statement, const std::string &context_name);

	std::vector<PathSection> copyLastSections(const PathsId &paths_id);
	bool isSectionExist(const PathSection &sections, int &sec_id);
	void clearExistingSections(PathsId &paths_id);

	IfCondition getIfConditions(const CXCursor &if_stmt);
	CallSequence getCallSequence(const CXCursor &call_expr);
	CircleHead getCircleCmpndStmt(const CXCursor &circle_stmt);
	void filterPaths(const Paths &orig_paths, Paths &ended_paths, Paths &paths_for_copy, const std::string &context_name);
	void filterPaths_(const PathsId &orig_paths, PathsId &ended_paths, PathsId &paths_for_copy, const std::string &context_name);

	void printPaths(const Paths &paths);

	std::vector<Path> paths;
	std::shared_ptr<Program> current_program;
	std::map<int, PathSection> path_sections;

	int getPathSectionId();
};