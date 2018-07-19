
#pragma once

#include <string>
#include <unordered_map>

#include "catalog/abstract_catalog.hpp"
#include "catalog/table_catalog.hpp"

namespace duckdb {

class Catalog;

class SchemaCatalogEntry : public AbstractCatalogEntry {
  public:
	SchemaCatalogEntry(std::string name);

	void CreateTable(const std::string &table_name,
	                 const std::vector<ColumnCatalogEntry> &columns);
	bool TableExists(const std::string &table_name);
	std::shared_ptr<TableCatalogEntry> GetTable(const std::string &table);

	std::unordered_map<std::string, std::shared_ptr<TableCatalogEntry>> tables;

	virtual std::string ToString() const { return std::string(); }
};
}