#define DUCKDB_EXTENSION_MAIN
#include "nanodbc_extension.hpp"
#include "duckdb.hpp"
#include "odbc_scanner.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

static void RegisterOdbcFunctions(ExtensionLoader &loader) {
    // Register each function directly via ExtensionLoader
    loader.RegisterFunction(OdbcScanFunction());
    loader.RegisterFunction(OdbcAttachFunction());
    loader.RegisterFunction(OdbcQueryFunction());
    loader.RegisterFunction(OdbcExecFunction());
}

static void LoadInternal(ExtensionLoader &loader) {
    // Register the ODBC functions
    RegisterOdbcFunctions(loader);
}

void NanodbcExtension::Load(ExtensionLoader &loader) {
    LoadInternal(loader);
}

} // namespace duckdb

extern "C" {

// Use the new macro for defining the entry point
DUCKDB_CPP_EXTENSION_ENTRY(nanodbc, loader) {
    duckdb::NanodbcExtension::Load(loader);
}

DUCKDB_EXTENSION_API const char *nanodbc_version() {
    return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif