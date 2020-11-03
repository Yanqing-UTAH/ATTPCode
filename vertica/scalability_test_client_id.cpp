#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <termios.h>
#include <unistd.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <chrono>
#include <dirent.h>

#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define STRINGIFY_HELPER(x) #x

#define TIMEIT(msg, statements) \
    { \
        std::cout << msg << " finished in "; \
        auto t1 = std::chrono::steady_clock::now(); \
        { statements } \
        auto t2 = std::chrono::steady_clock::now(); \
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>( \
            t2 - t1).count() << " ms" << std::endl; \
    }

void printError(SQLHSTMT hdlStmt) {
    SQLLEN numRecs = 0;
    SQLGetDiagField(SQL_HANDLE_STMT,
            hdlStmt, 0, SQL_DIAG_NUMBER, &numRecs, 0, 0);
    SQLCHAR SqlState[6];
    SQLINTEGER NativeError;
    SQLCHAR Msg[SQL_MAX_MESSAGE_LENGTH];
    SQLSMALLINT MsgLen;
    for (SQLLEN i = 1; i <= numRecs; ++i) {
        SQLRETURN ret = SQLGetDiagRec(SQL_HANDLE_STMT, hdlStmt, i, SqlState,
            &NativeError, Msg, sizeof(Msg), &MsgLen);
        if (!SQL_SUCCEEDED(ret)) {
            std::cerr << "[ERROR] error when printing error state "
                << i << std::endl;
            return ;
        }
        std::cerr << "[ERROR] " << Msg << std::endl;
    }
}

#define CHECK_OK_OR_PRINT() \
    do { \
        if (!SQL_SUCCEEDED(ret) || ret == SQL_SUCCESS_WITH_INFO) { \
            printError(hdlStmt); \
            goto cleanup; \
        } \
    } while (0)

int main(int argc, char *argv[]) {
    
    if (argc < 3) {
        std::cout << "usage: " << argv[0] << " <input_dir> <use_preaggregate:t/f>" << std::endl;
        return 1;
    }

    std::string input_dir = argv[1];
    bool use_preaggregate = argv[2][0] == 't';

    std::vector<int> days;
    DIR *dp;
    dirent *ep;
    dp = opendir(input_dir.c_str());
    if (!dp) {
        std::cerr << "[ERROR] unable to open input directory "
            << input_dir << std::endl;
        return 1;
    }
    while (ep = readdir(dp)) {
        if (ep->d_type != DT_REG) continue;
        std::string file_name = ep->d_name;
        if (file_name.length() >= 9 &&
            file_name.substr(0, 4) == "vday" &&
            file_name.substr(file_name.length() - 4) == ".dat") {
            int day = atoi(file_name.substr(4, file_name.length() - 8).c_str());
            days.push_back(day);
        }
    }
    std::sort(days.begin(), days.end());
    std::cout << "Found " << days.size() << " days of data" << std::endl;

    SQLRETURN ret;
    SQLHENV hdlEnv;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hdlEnv);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[ERROR] unable to allocate env handle" << std::endl;
        return 1;
    }

    ret = SQLSetEnvAttr(hdlEnv, SQL_ATTR_ODBC_VERSION,
            (SQLPOINTER) SQL_OV_ODBC3, SQL_IS_UINTEGER);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[ERROR] unable to set application version to ODBC 3"
            << std::endl;
        return 1;
    }

    SQLHDBC hdlDbc;
    ret = SQLAllocHandle(SQL_HANDLE_DBC, hdlEnv, &hdlDbc);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[ERROR] unable to allocate database handle"
            << std::endl;
        return 1;
    }
    
    std::cout << "Username: ";
    std::string username;
    std::getline(std::cin, username);
    
    struct termios t_old, t_new;
    tcgetattr(STDIN_FILENO, &t_old);
    t_new = t_old;
    t_new.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);
    std::cout << "Password: ";
    std::string passwd;
    std::getline(std::cin, passwd);
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
    std::cout << std::endl;

    const char *dsnName = "VerticaDSN";
    ret = SQLConnect(hdlDbc,
            (SQLCHAR*) dsnName, SQL_NTS,
            (SQLCHAR*) username.c_str(), SQL_NTS,
            (SQLCHAR*) passwd.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[ERROR] unable to connect to db" << std::endl;
        return 1;
    }

    std::cout << "Connected" << std::endl;

    SQLHSTMT hdlStmt ;
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdlDbc, &hdlStmt);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[ERROR} unable to allocate statement handle"
            << std::endl;
        return 1;
    }

    std::cout << "Heavy hitters on columnar store "
        << (use_preaggregate ? "w" : "w/o")
        << " pre-aggregates"
        << std::endl;
    TIMEIT("Drop Table wc_client_id", {
        ret = SQLExecDirect(hdlStmt, (SQLCHAR*)
            "drop table if exists wc_client_id",
            SQL_NTS);
    })
    CHECK_OK_OR_PRINT();
    
    if (use_preaggregate) {
        TIMEIT("Create Table wc_client_id", {
            ret = SQLExecDirect(hdlStmt, (SQLCHAR*)
                "create table wc_client_id ( "
                "ts int, "
                "cid int, "
                "cnt int)",
                SQL_NTS);
        });
    } else {
        TIMEIT("Create Table wc_client_id", {
            ret = SQLExecDirect(hdlStmt, (SQLCHAR*)
                "create table wc_client_id ( "
                "ts int, "
                "cid int)",
                SQL_NTS);
        })
    }
    CHECK_OK_OR_PRINT();
    
    std::cout << "Start loading and querying data" << std::endl;
    for (int day : days) {
        std::cout << "--day " <<  day << std::endl;
        std::string prefix = "vday";
        prefix += std::to_string(day);
        std::string dat_file = prefix + ".dat";
        std::string query_file = prefix + ".q";

        std::string load_target;
        if (use_preaggregate) {
            load_target = "wc_client_id_daily";
            
            // drop and create a new temporary table
            TIMEIT("Drop Table wc_client_id_daily", {
                ret = SQLExecDirect(hdlStmt, (SQLCHAR*)
                    "drop table if exists wc_client_id_daily",
                    SQL_NTS);
            });
            CHECK_OK_OR_PRINT();
            TIMEIT("Create Table wc_client_id_daily", {
                ret = SQLExecDirect(hdlStmt, (SQLCHAR*)
                    "create table wc_client_id_daily( "
                    "ts int, "
                    "cid int)",
                    SQL_NTS);
            });
            CHECK_OK_OR_PRINT();
        } else {
            load_target = "wc_client_id";
        }

        // load data
        std::string copy_stmt_str;
        copy_stmt_str.append("copy ")
            .append(load_target)
            .append(" from '")
            .append(input_dir)
            .append("/")
            .append(dat_file)
            .append("' delimiter ' '");
        TIMEIT("Copy", {
            ret = SQLExecDirect(hdlStmt,
                (SQLCHAR*) copy_stmt_str.c_str(), SQL_NTS);
        })
        CHECK_OK_OR_PRINT();
        //std::cerr << copy_stmt_str << std::endl;

        if (use_preaggregate) {
            // compute daily aggregates and insert into wc_client_id
            TIMEIT("Insert Into wc_client_id", {
                ret = SQLExecDirect(hdlStmt, (SQLCHAR*)
                    "insert into wc_client_id "
                    "select ts, cid, count(*) as cnt "
                    "from wc_client_id_daily "
                    "group by ts, cid",
                    SQL_NTS);
            });
            CHECK_OK_OR_PRINT();
        }
        
        // query
        std::ifstream fin(input_dir + "/" + query_file);
        std::string query_line;
        while (std::getline(fin, query_line)) {
            if (query_line.empty()) {
                continue;
            }
            auto p = query_line.find(' ');
            std::string ts_str = query_line.substr(0, p);
            std::string threshold_str = query_line.substr(p + 1);
            std::string query_stmt_str;
            std::string agg_expr =
                use_preaggregate ? "sum(cnt)" : "count(*)";
            query_stmt_str.append(
                "with t as ( "
                "   select * from wc_client_id where ts <= ").append(ts_str)
                .append(
                " ) "
                "select count(*) from ( "
                "   select cid, ").append(agg_expr).append(" as cnt "
                "   from t "
                "   group by cid "
                "   having ").append(agg_expr).append(" >= ")
                .append(threshold_str).append(" * ( "
                "       select ").append(agg_expr).append(" from t) "
                "   ) A");

            SQLBIGINT cnt;
            TIMEIT("Select", {
                ret = SQLExecDirect(hdlStmt,
                    (SQLCHAR*) query_stmt_str.c_str(), SQL_NTS);
                CHECK_OK_OR_PRINT();
                ret = SQLFetch(hdlStmt);
                CHECK_OK_OR_PRINT();
                ret = SQLGetData(hdlStmt, 1, SQL_C_SBIGINT, (SQLPOINTER) &cnt,
                    sizeof(cnt), nullptr);
                CHECK_OK_OR_PRINT();
                ret = SQLCloseCursor(hdlStmt);
                CHECK_OK_OR_PRINT();
            });
            std::cout << "day " << day << " ts (0, "
                << ts_str << "] #heavy_hitters = " << cnt << std::endl;
            ///std::cerr << query_stmt_str << std::endl;
        }
    }

cleanup:
    ret = SQLDisconnect(hdlDbc);
    if (!SQL_SUCCEEDED(ret)) {
        std::cerr << "[ERROR] unable to disconnect" << std::endl;
        return 1;
    }
    
    SQLFreeHandle(SQL_HANDLE_STMT, hdlStmt);
    SQLFreeHandle(SQL_HANDLE_DBC, hdlDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hdlEnv);
    return 0;
}
