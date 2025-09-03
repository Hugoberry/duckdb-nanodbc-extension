#pragma once

#include "duckdb.hpp"

namespace duckdb {

class ExtensionLoader; // Forward declaration

// Check if we're using the new extension API
#ifdef DUCKDB_CPP_EXTENSION_ENTRY
// New API - standalone class with static methods
class NanodbcExtension {
public:
    static void Load(ExtensionLoader &loader);
    
    static std::string Name() {
        return "nanodbc";
    }
    
    static std::string Version() {
#ifdef EXT_VERSION_NANODBC
        return EXT_VERSION_NANODBC;
#else
        return "v0.3.0";
#endif
    }
};

#else
// Old API - inherit from Extension base class
class NanodbcExtension : public Extension {
public:
    void Load(DuckDB &db) override;
    
    std::string Name() override {
        return "nanodbc";
    }
    
    std::string Version() const override {
#ifdef EXT_VERSION_NANODBC
        return EXT_VERSION_NANODBC;
#else
        return "v0.3.0";
#endif
    }
};
#endif

} // namespace duckdb