#define DUCKDB_EXTENSION_MAIN
#include "nanodbc_extension.hpp"
#include "duckdb.hpp"
#include "odbc_scanner.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#ifndef DUCKDB_CPP_EXTENSION_ENTRY
#include "duckdb/main/extension_util.hpp"
#endif

namespace duckdb {

// New API version - uses ExtensionLoader
#ifdef DUCKDB_CPP_EXTENSION_ENTRY
static void RegisterOdbcFunctions(ExtensionLoader &loader) {
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

#else
// Old API version - uses DatabaseInstance and ExtensionUtil
static void RegisterOdbcFunctions(DatabaseInstance &instance) {
    ExtensionUtil::RegisterFunction(instance, OdbcScanFunction());
    ExtensionUtil::RegisterFunction(instance, OdbcAttachFunction());
    ExtensionUtil::RegisterFunction(instance, OdbcQueryFunction());
    ExtensionUtil::RegisterFunction(instance, OdbcExecFunction());
}

static void LoadInternal(DatabaseInstance &instance) {
    // Register the ODBC functions
    RegisterOdbcFunctions(instance);
}

void NanodbcExtension::Load(DuckDB &db) {
    LoadInternal(*db.instance);
}
#endif

} // namespace duckdb

extern "C" {

#ifdef DUCKDB_CPP_EXTENSION_ENTRY
// New API entry point
DUCKDB_CPP_EXTENSION_ENTRY(nanodbc, loader) {
    duckdb::NanodbcExtension::Load(loader);
}
#else
// Old API entry point
DUCKDB_EXTENSION_API void nanodbc_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<duckdb::NanodbcExtension>();
}
#endif

DUCKDB_EXTENSION_API const char *nanodbc_version() {
    return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif