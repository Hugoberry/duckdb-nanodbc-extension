#pragma once

#include "duckdb.hpp"
#include "odbc_headers.hpp"
#include <string>
#include <unordered_map>

namespace duckdb {

class OdbcUtils {
public:
    // Format error message and throw a DuckDB exception
    static void ThrowException(const std::string& operation, const nanodbc::database_error& e);
    
    // Sanitize string for ODBC usage (escape quotes)
    static std::string SanitizeString(const std::string& input);
    
    // Get column metadata from nanodbc result
    static void GetColumnMetadata(nanodbc::result& result, idx_t colIdx, 
                                 SQLSMALLINT& type, SQLULEN& columnSize, SQLSMALLINT& decimalDigits);
    
    // Type conversion lookups
    static LogicalType OdbcTypeToLogicalType(SQLSMALLINT odbcType, SQLULEN columnSize, SQLSMALLINT decimalDigits);
    static SQLSMALLINT LogicalTypeToOdbcType(const LogicalType& type);
    
    // Helper methods for data handling
    static bool IsBinaryType(SQLSMALLINT sqlType);
    static bool ReadVarData(nanodbc::result& result, idx_t colIdx, bool& isNull, std::vector<char>& output);
    
    // Type name for error messages
    static std::string GetTypeName(SQLSMALLINT odbcType);

    static bool IsVarcharType(SQLSMALLINT sqlType);
#ifdef _WIN32
    static std::string ConvertToUTF8(const std::string& input, int codepage);
#endif
private:
    // Lookup tables for type conversion
    static const std::unordered_map<SQLSMALLINT, LogicalTypeId> ODBC_TO_DUCKDB_TYPES;
    static const std::unordered_map<LogicalTypeId, SQLSMALLINT> DUCKDB_TO_ODBC_TYPES;
    static const std::unordered_map<SQLSMALLINT, std::string> TYPE_NAMES;
};

} // namespace duckdb