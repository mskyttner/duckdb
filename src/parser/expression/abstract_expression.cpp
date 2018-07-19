
#include "parser/expression/abstract_expression.hpp"

using namespace duckdb;
using namespace std;

bool AbstractExpression::IsAggregate() {
	bool is_aggregate = false;
	for(auto& child : children) {
		is_aggregate |= child->IsAggregate();
	}
	return is_aggregate;
}