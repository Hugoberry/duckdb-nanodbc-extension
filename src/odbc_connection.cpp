#include "odbc_connection.hpp"
#include "odbc_statement.hpp"
#include "odbc_utils.hpp"
#include "duckdb/parser/column_list.hpp"
#include "duckdb/parser/constraints/not_null_constraint.hpp"
#include "duckdb/parser/constraints/unique_constraint.hpp"

namespace duckdb {

//---------------------------------------------------------------------------
// ConnectionParams implementation
//---------------------------------------------------------------------------

ConnectionParams::ConnectionParams(std::string connection_info, 
                                 std::string username,
                                 std::string password,
                                 int timeout,
                                 bool read_only)
    : username(std::move(username))
    , password(std::move(password))
    , timeout(timeout)
    , read_only(read_only) {
    
    // Determine if this is a DSN or connection string
    if (connection_info.find('=') == std::string::npos) {
        // Likely a DSN
        dsn = std::move(connection_info);
        is_dsn = true;
    } else {
        // Connection string
        connection_string = std::move(connection_info);
        is_dsn = false;
    }
}

bool ConnectionParams::IsValid() const {
    return !dsn.empty() || !connection_string.empty();
}

std::string ConnectionParams::GetConnectionString() const {
    if (!is_dsn) {
        return connection_string;
    }
    
    // For DSN, we don't generate a connection string
    // This is handled by nanodbc's connection constructor
    return std::string();
}

//---------------------------------------------------------------------------
// OdbcConnection implementation
//---------------------------------------------------------------------------

OdbcConnection::~OdbcConnection() {
    // Close connection if open
    if (IsOpen()) {
        try {
            connection.disconnect();
        } catch (...) {
            // Ignore exceptions during disconnect
        }
    }
}

OdbcConnection::OdbcConnection(OdbcConnection &&other) noexcept {
    connection = std::move(other.connection);
}

OdbcConnection &OdbcConnection::operator=(OdbcConnection &&other) noexcept {
    if (this != &other) {
        // Close current connection if open
        if (IsOpen()) {
            try {
                connection.disconnect();
            } catch (...) {
                // Ignore exceptions during disconnect
            }
        }
        
        // Move the connection
        connection = std::move(other.connection);
    }
    return *this;
}

unique_ptr<OdbcConnection> OdbcConnection::Connect(const ConnectionParams& params) {
    if (!params.IsValid()) {
        throw BinderException("No valid connection information provided");
    }
    
    auto db = make_uniq<OdbcConnection>();
    
    try {
        // Connect to the data source
        if (params.GetDsn().empty()) {
            // Connect via connection string
            db->connection = nanodbc::connection(params.GetConnectionString(), params.GetTimeout());
        } else {
            // Connect via DSN
            if (params.GetUsername().empty() && params.GetPassword().empty()) {
                db->connection = nanodbc::connection(params.GetDsn(), "", "", params.GetTimeout());
            } else {
                db->connection = nanodbc::connection(params.GetDsn(), params.GetUsername(), 
                                                   params.GetPassword(), params.GetTimeout());
            }
        }
        
        // Set read-only mode if requested
        if (params.IsReadOnly()) {
            try {
                SQLHDBC nativeHandle = db->connection.native_dbc_handle();
                SQLSetConnectAttr(nativeHandle, SQL_ATTR_ACCESS_MODE, (SQLPOINTER)SQL_MODE_READ_ONLY, 0);
            } catch (...) {
                // Just ignore if read-only setting fails
            }
        }
        
        return db;
    } catch (const nanodbc::database_error& e) {
        OdbcUtils::ThrowException(params.GetDsn().empty() ? 
                                "connect with connection string" : 
                                "connect to DSN '" + params.GetDsn() + "'", e);
        return nullptr; // Won't reach here due to exception
    }
}

unique_ptr<OdbcStatement> OdbcConnection::Prepare(const std::string &query) {
    if (!IsOpen()) {
        throw BinderException("Cannot prepare statement: connection is closed");
    }
    
    try {
        return make_uniq<OdbcStatement>(connection, query);
    } catch (const nanodbc::database_error &e) {
        OdbcUtils::ThrowException("prepare query \"" + query + "\"", e);
        return nullptr; // Won't reach here due to exception
    }
}

void OdbcConnection::Execute(const std::string &query) {
    try {
        nanodbc::just_execute(connection, query);
    } catch (const nanodbc::database_error& e) {
        OdbcUtils::ThrowException("execute query \"" + query + "\"", e);
    }
}

bool OdbcConnection::IsOpen() const {
    return connection.connected();
}

std::vector<std::string> OdbcConnection::GetTables() {
    std::vector<std::string> tables;
    
    try {
        // Use nanodbc's catalog functions to get tables
        nanodbc::catalog catalog(connection);
        auto tableResults = catalog.find_tables(std::string(), std::string("TABLE"), std::string(), std::string());
        
        while (tableResults.next()) {
            std::string tableName = tableResults.table_name();
            tables.push_back(tableName);
        }
    } catch (const nanodbc::database_error& e) {
        OdbcUtils::ThrowException("get table list", e);
    }
    
    return tables;
}

void OdbcConnection::GetTableInfo(const std::string &tableName, ColumnList &columns, 
                                std::vector<std::unique_ptr<Constraint>> &constraints, bool allVarchar) {
    try {
        // Get column information using nanodbc catalog
        nanodbc::catalog catalog(connection);
        auto columnResults = catalog.find_columns(std::string(), tableName, std::string(), std::string());

        idx_t columnIndex = 0;
        
        while (columnResults.next()) {
            std::string name = columnResults.column_name();
            SQLSMALLINT dataType = columnResults.data_type();
            SQLULEN columnSize = 0;
            // Get column size safely - catch exceptions for VARCHAR types
            try {
                columnSize = columnResults.column_size();
            } catch (const std::exception& e) {
                // If column_size() fails, use a safe default (for VARCHAR in DuckDB)
                columnSize = 0; // Will be handled when converting to logical type
            }
            SQLSMALLINT decimalDigits = columnResults.decimal_digits();
            SQLSMALLINT nullable = columnResults.nullable();
            
            LogicalType columnType;
            if (allVarchar) {
                columnType = LogicalType::VARCHAR;
            } else {
                // For VARCHAR types in DuckDB, don't rely on column size
                if (OdbcUtils::IsVarcharType(dataType)) {
                    columnType = LogicalType::VARCHAR;
                } else {
                    columnType = OdbcUtils::OdbcTypeToLogicalType(dataType, columnSize, decimalDigits);
                }
            }
            
            ColumnDefinition column(std::move(name), columnType);
            columns.AddColumn(std::move(column));
            
            if (nullable == SQL_NO_NULLS) {
                constraints.push_back(make_uniq<NotNullConstraint>(LogicalIndex(columnIndex)));
            }
            
            columnIndex++;
        }
        
        if (columnIndex == 0) {
            throw BinderException("No columns found for table '" + tableName + "'");
        }

    } catch (const nanodbc::database_error& e) {
        OdbcUtils::ThrowException("get table info for '" + tableName + "'", e);
    }
}

std::vector<std::string> OdbcConnection::GetViews() {
    std::vector<std::string> views;
    
    try {
        // Use nanodbc's catalog functions to get views
        nanodbc::catalog catalog(connection);
        
        // VIEW type for standard ODBC
        auto viewResults = catalog.find_tables(std::string(), std::string("VIEW"), std::string(), std::string());
        
        while (viewResults.next()) {
            std::string viewName = viewResults.table_name();
            views.push_back(viewName);
        }
        
        // Some databases might use different types for views
        try {
            // Try for databases that use "SYSTEM VIEW" type
            auto sysViewResults = catalog.find_tables(std::string(), std::string("SYSTEM VIEW"), std::string(), std::string());
            
            while (sysViewResults.next()) {
                std::string viewName = sysViewResults.table_name();
                views.push_back(viewName);
            }
        } catch (...) {
            // Ignore if this fails - just continue with the views we've found
        }
        
    } catch (const nanodbc::database_error& e) {
        // Just log the error rather than failing completely
        // This allows partial attachment to work
        fprintf(stderr, "Warning: Could not get views: %s\n", e.what());
    }
    
    return views;
}

} // namespace duckdb