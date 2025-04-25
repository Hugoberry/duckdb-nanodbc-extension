#include "odbc_utils.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

// Type conversion lookup tables
const std::unordered_map<SQLSMALLINT, LogicalTypeId> OdbcUtils::ODBC_TO_DUCKDB_TYPES = {
    {SQL_BIT, LogicalTypeId::BOOLEAN},
    {SQL_TINYINT, LogicalTypeId::TINYINT},
    {SQL_SMALLINT, LogicalTypeId::SMALLINT},
    {SQL_INTEGER, LogicalTypeId::INTEGER},
    {SQL_BIGINT, LogicalTypeId::BIGINT},
    {SQL_REAL, LogicalTypeId::FLOAT},
    {SQL_FLOAT, LogicalTypeId::FLOAT},
    {SQL_DOUBLE, LogicalTypeId::DOUBLE},
    {SQL_DECIMAL, LogicalTypeId::DECIMAL},
    {SQL_NUMERIC, LogicalTypeId::DECIMAL},
    {SQL_CHAR, LogicalTypeId::VARCHAR},
    {SQL_VARCHAR, LogicalTypeId::VARCHAR},
    {SQL_LONGVARCHAR, LogicalTypeId::VARCHAR},
    {SQL_WCHAR, LogicalTypeId::VARCHAR},
    {SQL_WVARCHAR, LogicalTypeId::VARCHAR},
    {SQL_WLONGVARCHAR, LogicalTypeId::VARCHAR},
    {SQL_BINARY, LogicalTypeId::BLOB},
    {SQL_VARBINARY, LogicalTypeId::BLOB},
    {SQL_LONGVARBINARY, LogicalTypeId::BLOB},
    {SQL_DATE, LogicalTypeId::DATE},
    {SQL_TYPE_DATE, LogicalTypeId::DATE},
    {SQL_TIME, LogicalTypeId::TIME},
    {SQL_TYPE_TIME, LogicalTypeId::TIME},
    {SQL_TIMESTAMP, LogicalTypeId::TIMESTAMP},
    {SQL_TYPE_TIMESTAMP, LogicalTypeId::TIMESTAMP},
    {SQL_GUID, LogicalTypeId::UUID}
};

const std::unordered_map<LogicalTypeId, SQLSMALLINT> OdbcUtils::DUCKDB_TO_ODBC_TYPES = {
    {LogicalTypeId::BOOLEAN, SQL_BIT},
    {LogicalTypeId::TINYINT, SQL_TINYINT},
    {LogicalTypeId::SMALLINT, SQL_SMALLINT},
    {LogicalTypeId::INTEGER, SQL_INTEGER},
    {LogicalTypeId::BIGINT, SQL_BIGINT},
    {LogicalTypeId::FLOAT, SQL_REAL},
    {LogicalTypeId::DOUBLE, SQL_DOUBLE},
    {LogicalTypeId::DECIMAL, SQL_DECIMAL},
    {LogicalTypeId::VARCHAR, SQL_VARCHAR},
    {LogicalTypeId::BLOB, SQL_VARBINARY},
    {LogicalTypeId::DATE, SQL_TYPE_DATE},
    {LogicalTypeId::TIME, SQL_TYPE_TIME},
    {LogicalTypeId::TIMESTAMP, SQL_TYPE_TIMESTAMP},
    {LogicalTypeId::UUID, SQL_GUID},
    {LogicalTypeId::UTINYINT, SQL_TINYINT},
    {LogicalTypeId::USMALLINT, SQL_SMALLINT},
    {LogicalTypeId::UINTEGER, SQL_INTEGER},
    {LogicalTypeId::UBIGINT, SQL_BIGINT}
};

const std::unordered_map<SQLSMALLINT, std::string> OdbcUtils::TYPE_NAMES = {
    {SQL_CHAR, "CHAR"},
    {SQL_VARCHAR, "VARCHAR"},
    {SQL_LONGVARCHAR, "LONGVARCHAR"},
    {SQL_WCHAR, "WCHAR"},
    {SQL_WVARCHAR, "WVARCHAR"},
    {SQL_WLONGVARCHAR, "WLONGVARCHAR"},
    {SQL_DECIMAL, "DECIMAL"},
    {SQL_NUMERIC, "NUMERIC"},
    {SQL_SMALLINT, "SMALLINT"},
    {SQL_INTEGER, "INTEGER"},
    {SQL_REAL, "REAL"},
    {SQL_FLOAT, "FLOAT"},
    {SQL_DOUBLE, "DOUBLE"},
    {SQL_BIT, "BIT"},
    {SQL_TINYINT, "TINYINT"},
    {SQL_BIGINT, "BIGINT"},
    {SQL_BINARY, "BINARY"},
    {SQL_VARBINARY, "VARBINARY"},
    {SQL_LONGVARBINARY, "LONGVARBINARY"},
    {SQL_DATE, "DATE"},
    {SQL_TIME, "TIME"},
    {SQL_TIMESTAMP, "TIMESTAMP"},
    {SQL_TYPE_DATE, "DATE"},
    {SQL_TYPE_TIME, "TIME"},
    {SQL_TYPE_TIMESTAMP, "TIMESTAMP"},
    {SQL_GUID, "GUID"}
};

void OdbcUtils::ThrowException(const std::string& operation, const nanodbc::database_error& e) {
    throw BinderException("ODBC error: Failed to " + operation + ": " + e.what());
}

std::string OdbcUtils::SanitizeString(const std::string& input) {
    return StringUtil::Replace(input, "\"", "\"\"");
}

std::string OdbcUtils::GetTypeName(SQLSMALLINT odbcType) {
    auto it = TYPE_NAMES.find(odbcType);
    if (it != TYPE_NAMES.end()) {
        return it->second;
    }
    return "UNKNOWN";
}

LogicalType OdbcUtils::OdbcTypeToLogicalType(SQLSMALLINT odbcType, SQLULEN columnSize, SQLSMALLINT decimalDigits) {
    auto it = ODBC_TO_DUCKDB_TYPES.find(odbcType);
    if (it != ODBC_TO_DUCKDB_TYPES.end()) {
        LogicalTypeId typeId = it->second;
        
        // Special handling for decimal
        if (typeId == LogicalTypeId::DECIMAL) {
            if (columnSize == 0) columnSize = 38;  // Default precision
            // if (decimalDigits == 0 && odbcType == SQL_DECIMAL) decimalDigits = 2;  // Default scale
            return LogicalType::DECIMAL(columnSize, decimalDigits);
        }
        
        return LogicalType(typeId);
    }
    
    // Default to VARCHAR for unknown types
    return LogicalType::VARCHAR;
}

SQLSMALLINT OdbcUtils::LogicalTypeToOdbcType(const LogicalType& type) {
    auto it = DUCKDB_TO_ODBC_TYPES.find(type.id());
    if (it != DUCKDB_TO_ODBC_TYPES.end()) {
        return it->second;
    }
    return SQL_VARCHAR; // Default for unknown types
}

bool OdbcUtils::IsBinaryType(SQLSMALLINT sqlType) {
    return sqlType == SQL_BINARY || sqlType == SQL_VARBINARY || sqlType == SQL_LONGVARBINARY;
}

void OdbcUtils::GetColumnMetadata(nanodbc::result& result, idx_t colIdx, 
                                SQLSMALLINT& type, SQLULEN& columnSize, SQLSMALLINT& decimalDigits) {
    try {
        // Get the column data type
        type = result.column_datatype(colIdx);
        
        // Get the column size and decimal digits
        columnSize = 0;
        decimalDigits = 0;
        
        // For some data types, we need additional metadata
        if (type == SQL_NUMERIC || type == SQL_DECIMAL) {
            columnSize = result.column_size(colIdx);
            decimalDigits = result.column_decimal_digits(colIdx);
        } else if (type == SQL_CHAR || type == SQL_VARCHAR || type == SQL_WCHAR || type == SQL_WVARCHAR) {
            columnSize = result.column_size(colIdx);
        } else if (type == SQL_BINARY || type == SQL_VARBINARY) {
            columnSize = result.column_size(colIdx);
        }
    } catch (const nanodbc::database_error& e) {
        ThrowException("get column metadata", e);
    }
}

bool OdbcUtils::ReadVarData(nanodbc::result& result, idx_t colIdx, bool& isNull, std::vector<char>& output) {
    isNull = false;
    output.clear();
    
    try {
        if (result.is_null(colIdx)) {
            isNull = true;
            return true;
        }
        
        // Get the data using nanodbc
        std::string value = result.get<std::string>(colIdx);
        
        // Copy to output vector
        output.assign(value.begin(), value.end());
        return true;
    } catch (const nanodbc::database_error& e) {
        ThrowException("read variable data", e);
        return false; // Won't reach here due to exception
    }
}

bool OdbcUtils::IsVarcharType(SQLSMALLINT sqlType) {
    return sqlType == SQL_CHAR || sqlType == SQL_VARCHAR || 
           sqlType == SQL_LONGVARCHAR || sqlType == SQL_WCHAR || 
           sqlType == SQL_WVARCHAR || sqlType == SQL_WLONGVARCHAR;
}

#ifdef _WIN32
std::string OdbcUtils::ConvertToUTF8(const std::string& input, int codepage) {
    if (input.empty()) {
        return input;
    }
    
    // First, convert from specified codepage to UTF-16
    int wide_size = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS,
                                      input.c_str(), -1, nullptr, 0);
    
    if (wide_size == 0) {
        // If conversion fails, return original string
        return input;
    }
    
    std::vector<wchar_t> wide_str(wide_size);
    if (MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, 
                           input.c_str(), -1, 
                           wide_str.data(), wide_size) == 0) {
        // If conversion fails, return original string
        return input;
    }
    
    // Then convert from UTF-16 to UTF-8
    int utf8_size = WideCharToMultiByte(CP_UTF8, 0,
                                       wide_str.data(), -1,
                                       nullptr, 0, nullptr, nullptr);
    
    if (utf8_size == 0) {
        // If conversion fails, return original string
        return input;
    }
    
    std::vector<char> utf8_str(utf8_size);
    if (WideCharToMultiByte(CP_UTF8, 0,
                           wide_str.data(), -1,
                           utf8_str.data(), utf8_size,
                           nullptr, nullptr) == 0) {
        // If conversion fails, return original string
        return input;
    }
    
    // Remove null terminator if present
    if (utf8_size > 0 && utf8_str[utf8_size - 1] == '\0') {
        return std::string(utf8_str.data(), utf8_size - 1);
    }
    
    return std::string(utf8_str.data(), utf8_size);
}
#endif

} // namespace duckdb