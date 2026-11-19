#ifndef DATABASE__H_
#define DATABASE__H_

//#define USE_MYSQL 1   // MySQL にするときは有効にする（接続先は環境変数 RDS_PROXY_HOST など）
#define USE_MYSQL_ENV 1

#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<ctime>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>
#include <memory>

#include "json.hpp"

#ifdef USE_MYSQL
#include <sqlpp11_mysql_mysql.h>
#else
#include <sqlpp11_sqlite3_sqlite3.h>
#endif
#include <sqlpp11_sqlpp11.h>



using json = nlohmann::json;
#ifdef USE_MYSQL
namespace sql = sqlpp::mysql;
#else
namespace sql = sqlpp::sqlite3;
#endif



static inline std::string getenv_(const char* str)
{
        std::string ret;
        char* p;
        p=::getenv(str);
        if (p) {
            ret = p;
        }
        return ret;
}
static inline void db_global_init()
{
        static bool f_init = false;
        if (!f_init) {
            f_init = true;
#ifdef USE_MYSQL
            sql::global_library_init();
#endif
        }
}

// 設定は最初の 1 回だけ作る（複数スレッドから同時に呼ばれても安全なように関数内 static で）
static inline sql::connection_config db_make_config()
{
        sql::connection_config config;
        db_global_init();
#ifdef USE_MYSQL

#ifdef  USE_MYSQL_ENV
        config.host = getenv_("RDS_PROXY_HOST"); //"xxxx.rds.amazonaws.com";
        config.user = getenv_("USER_NAME"); //"xxxx";
        config.password = getenv_("PASSWORD"); // "xxxx";
        config.database = getenv_("DB_NAME"); // "xxxx";
        config.debug = false;

        if (config.host == "")config.host = "xxxx.rds.amazonaws.com";
        if(config.user=="")config.user = "xxxx";
        if(config.password=="")config.password = "xxxx";
        if(config.database=="")config.database = "xxxx";

        // TiDB は port 4000・TLS 必須。port は env TIDB_PORT で上書き可（既定4000）。
        {
            std::string pe = getenv_("TIDB_PORT");
            config.port = pe.empty() ? 4000u : (unsigned int)atoi(pe.c_str());
        }
        config.client_flag |= CLIENT_SSL;   // TLS を要求（プロトコル標準フラグ）
        // MariaDB 互換 libmysqlclient は MYSQL_OPT_RECONNECT の enum 値が MySQL と違い
        // mysql_options がエラー→sqlpp11 が例外で落ちる。auto_reconnect を切って回避。
        config.auto_reconnect = false;
#else

        config.user = "root";
        config.database = "sakila";
        config.debug = false;
        config.password = "xxxx";
        config.port = 3306;
#endif

#else
        config.path_to_database = "test_db.db";
        config.flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        config.debug = true;
#endif
        return config;
}

static inline const sql::connection_config& db_get_config()
{
        static const sql::connection_config config = db_make_config();
        return config;
}

#ifdef USE_MYSQL
// MySQL(TiDB) はスレッドごとに接続を 1 本持つ（httplib のワーカースレッドごと）。
// 1 本を全員で順番に使うと、遠い DB（Render オレゴン→TiDB 東京で 1 往復 100ms 超）では
// リクエストが詰まり、ヘルスチェックまで待たされて Render に再起動され 502 になった。
//   ・しばらく使っていない接続は相手（TiDB）に切られているかもしれないので張り直す
//   ・DB のエラーが出た接続は db_reset_connection() で捨て、次のリクエストで張り直す
static const time_t DB_IDLE_RECONNECT_SEC = 240;

struct db_thread_conn {
        std::unique_ptr<sql::connection> conn;
        time_t last_used = 0;
};

static inline db_thread_conn& db_thread_slot()
{
        thread_local db_thread_conn slot;
        return slot;
}

static inline sql::connection& get_sql_connection()
{
        db_thread_conn& s = db_thread_slot();
        time_t now = time(NULL);
        if (s.conn && now - s.last_used > DB_IDLE_RECONNECT_SEC) s.conn.reset();
        if (!s.conn) s.conn = std::make_unique<sql::connection>(db_get_config());
        s.last_used = now;
        return *s.conn;
}

static inline void db_reset_connection()
{
        db_thread_slot().conn.reset();
}
#else
// SQLite（ローカル用）は 1 本を共有し、呼ぶ側（run）で API 1 回分を排他にする
static inline sql::connection& get_sql_connection()
{
    static sql::connection db_a(db_get_config());
    return db_a;
}

static inline void db_reset_connection()
{
}
#endif

#endif

