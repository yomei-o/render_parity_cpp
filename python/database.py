import os
from sqlalchemy import create_engine

from sqlalchemy.orm import declarative_base
from sqlalchemy.orm import sessionmaker

USER_NAME = "xxxx"
PASSWORD = "xxxx"
RDS_PROXY_HOST = ""
DB_NAME = "xxxx"
TIDB_PORT = "4000"   # TiDB は 4000（通常の MySQL は 3306）

if val := os.environ.get("USER_NAME"):
    USER_NAME = val
if val := os.environ.get("PASSWORD"):
    PASSWORD = val
if val := os.environ.get("RDS_PROXY_HOST"):
    RDS_PROXY_HOST = val
if val := os.environ.get("DB_NAME"):
    DB_NAME = val
if val := os.environ.get("TIDB_PORT"):
    TIDB_PORT = val


# 既定はカレントディレクトリの SQLite（C++ 版と同じ test_db.db）。
# 環境変数 RDS_PROXY_HOST があれば MySQL / TiDB に繋ぐ（ポート既定 4000）。
if RDS_PROXY_HOST:
    SQLALCHEMY_DATABASE_URL = (
        f"mysql+pymysql://{USER_NAME}:{PASSWORD}@{RDS_PROXY_HOST}:{TIDB_PORT}/{DB_NAME}?charset=utf8mb4"
    )
else:
    SQLALCHEMY_DATABASE_URL = "sqlite:///./test_db.db"

# DATABASE_URL を指定するとそのURLをそのまま使う（どちらよりも優先）
#   例: DATABASE_URL=sqlite:///./test_db.db
if val := os.environ.get("DATABASE_URL"):
    SQLALCHEMY_DATABASE_URL = val

# パスワードはログに出さない
print(SQLALCHEMY_DATABASE_URL.replace(PASSWORD, "****") if PASSWORD else SQLALCHEMY_DATABASE_URL)

# 接続プールの健全性オプション（環境変数でオプトイン。未設定なら従来挙動のまま＝他プロジェクトに影響なし）
#   DB_POOL_PRE_PING=1   : チェックアウト時に接続の生存確認(ping)を行い、死んでいれば透過的に張り直す。
#                          Lambdaのフリーズ中に切れたプール接続を再利用して起きるエラー
#                          （Packet sequence number wrong / MySQL server has gone away）への対策。
#                          コストは同一リージョンRDSで1〜5ms/チェックアウト程度。
#   DB_POOL_RECYCLE=<秒> : 指定秒より古い接続をチェックアウト時に張り直す（例: 280）。
#   ※どちらもチェックアウト時のみ作用し、使用中(保持中)の接続には触れない（GET_LOCK運用に影響なし）。
_pool_kwargs = {}
if os.environ.get("DB_POOL_PRE_PING") == "1":
    _pool_kwargs["pool_pre_ping"] = True
if val := os.environ.get("DB_POOL_RECYCLE"):
    _pool_kwargs["pool_recycle"] = int(val)

# SQLite はスレッド間で接続を共有できないため check_same_thread を切る
if SQLALCHEMY_DATABASE_URL.startswith("sqlite"):
    _pool_kwargs["connect_args"] = {"check_same_thread": False}
# TiDB/MySQL は TLS 必須。システムCAで検証（TiDB Serverless は公的CA署名）。
# TIDB_SSL_SKIP_VERIFY=1 で検証を外せる（自己署名のMySQL等の緊急回避用）。
elif SQLALCHEMY_DATABASE_URL.startswith("mysql"):
    import ssl as _ssl
    _ctx = _ssl.create_default_context()
    if os.environ.get("TIDB_SSL_SKIP_VERIFY") == "1":
        _ctx.check_hostname = False
        _ctx.verify_mode = _ssl.CERT_NONE
    _pool_kwargs["connect_args"] = {"ssl": _ctx}
    # 既定のプール（常駐 5 本＋はみ出し分は使うたびに張って閉じる）だと、ページが API を 6 本以上
    # 同時に投げたとき毎回 TLS 接続を張り直し、遠い TiDB では 1 本あたり +0.8 秒ほどかかった。
    # 常駐を増やし、TiDB 側にアイドルで切られる前に張り直す（env で上書き可）。
    _pool_kwargs["pool_size"] = int(os.environ.get("DB_POOL_SIZE", "16"))
    _pool_kwargs["max_overflow"] = int(os.environ.get("DB_MAX_OVERFLOW", "16"))
    _pool_kwargs.setdefault("pool_recycle", 240)

engine = create_engine(SQLALCHEMY_DATABASE_URL, **_pool_kwargs)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

Base = declarative_base()

