# Render の Docker ランタイム用。cpp-httplib サーバ（既定 SQLite）をビルドして動かす。
# dockerContext は ./cpp なので、このディレクトリ一式が COPY される。
#
# DB: 既定は SQLite（Render の無料枠はディスク揮発なので再デプロイで消える）。
#     永続化は TiDB Cloud(MySQL)。その場合は USE_MYSQL ビルド＋libmysqlclient が要る（末尾コメント）。
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
# SQLite 版。重要: sqlite3.c は C。g++ は .c も C++ 扱いするので、gcc(C) で別オブジェクトにして
# から g++(C++) の本体とリンクする。mbedtls は mbedtlspp.hpp 経由のユニティ include。
RUN gcc -O2 -c sqlite/sqlite3.c -o sqlite3.o -I sqlite
RUN g++ -O2 -o lambda_server main/lambda_server.cpp sqlite3.o \
      -pthread --std=c++2a \
      -I main -I gen -I date -I models -I sqlite -I sqlpp11 -I include -I openssl \
      -ldl -lm

FROM debian:bookworm-slim
RUN useradd -m app
WORKDIR /app
COPY --from=build /src/lambda_server /app/lambda_server
COPY --from=build /src/html /app/html
USER app
EXPOSE 10000
# Render は PORT を注入する。サーバは環境変数 POST を読むので橋渡しする。
CMD ["/bin/sh", "-c", "POST=${PORT:-8080} exec /app/lambda_server"]

# --- TiDB Cloud(MySQL) で動かすときの参考 ---
# build ステージに libmysqlclient を入れ、USE_MYSQL でビルドし、gen/mysql_dynamic_loading.cpp を足す:
#   RUN apt-get update && apt-get install -y g++ default-libmysqlclient-dev
#   RUN g++ -O2 -DUSE_MYSQL=1 -o lambda_server main/lambda_server.cpp gen/mysql_dynamic_loading.cpp \
#       -pthread -ldl --std=c++2a -I main -I gen -I date -I models -I mysql_8_4 -I sqlpp11 -I include -I openssl
# 実行ステージに libmysqlclient ランタイム(libmariadb/ libmysqlclient .so)を入れ、
# 環境変数 RDS_PROXY_HOST / USER_NAME / PASSWORD / DB_NAME を Render で設定する。TiDB は TLS 必須。
