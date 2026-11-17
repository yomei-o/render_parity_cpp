# Render の Docker ランタイム用。cpp-httplib サーバを MySQL(TiDB) バックエンドでビルドして動かす。
# dockerContext は ./cpp なので、このディレクトリ一式が COPY される。
#
# DB: TiDB Cloud(MySQL互換, TLS必須, port 4000)。接続は環境変数
#     RDS_PROXY_HOST / TIDB_PORT / USER_NAME / PASSWORD / DB_NAME（Render で設定、database.h が読む）。
# MySQL クライアントは実行時に dlopen("libmysqlclient.so")。Debian は MariaDB 互換(libmariadb)で代替する。
# （SQLite 版に戻すときは USE_MYSQL を外し、sqlite3.c を gcc(C) で別コンパイルしてリンクする）
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
# USE_MYSQL ビルド。MySQL クライアントは dlopen なのでビルド時はヘッダ(mysql_8_4)だけでよい。
RUN g++ -O2 -DUSE_MYSQL -o lambda_server main/lambda_server.cpp gen/mysql_dynamic_loading.cpp \
      -pthread -ldl --std=c++2a \
      -I main -I gen -I date -I models -I mysql_8_4 -I sqlpp11 -I include -I openssl

FROM debian:bookworm-slim
# 実行時に dlopen する libmysqlclient.so を MariaDB 互換で用意（dev-compat が libmysqlclient.so の別名を提供）
RUN apt-get update && apt-get install -y --no-install-recommends \
      libmariadb3 libmariadb-dev-compat ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN useradd -m app
WORKDIR /app
COPY --from=build /src/lambda_server /app/lambda_server
COPY --from=build /src/html /app/html
RUN chown -R app:app /app
USER app
EXPOSE 10000
# Render は PORT を注入する。サーバは環境変数 POST を読むので橋渡しする。
CMD ["/bin/sh", "-c", "POST=${PORT:-8080} exec /app/lambda_server"]
