# render_parity_cpp

パスキー（WebAuthn）サーバの軽量サンプルを、**同じ API を FastAPI(Python) と cpp-httplib(C++) の
両方**で実装し、**Render.com** にデプロイするための入れ物。DB は **TiDB Cloud（MySQL 互換）** を予定。

元サンプル: [yomei-o/passkey_server_cpp](https://github.com/yomei-o/passkey_server_cpp) の `src/server_sample`。
Python/C++ の**機能パリティ**を保つのが方針（速度は別）。

## 動いているサーバ（Render 無料枠）

| 実装 | URL |
|---|---|
| C++（cpp-httplib, Docker） | https://render-parity-cpp.onrender.com/html/index.html |
| Python（FastAPI） | https://render-parity-python.onrender.com/html/index.html |

両方とも同じ TiDB Cloud を DB に使っている。無料枠なので 15 分アクセスが無いと止まり、
次の最初のアクセスは起動待ちになる（実測: C++ 約 13 秒、Python 約 33〜37 秒）。

## 構成

```
render.yaml       Render Blueprint（python と cpp の2サービス）
cpp/              cpp-httplib 実装（httplib + 自作mbedtls + sqlpp11 + sqlite3 同梱）
  main/           lambda_server.cpp（エントリ）, API(testsv_api_*), httplib/json
  include/        mbedtls（mbedtlspp 経由でユニティ include）
  sqlite/ sqlpp11/ mysql_8_4/ postgre_17/ models/ gen/ date/
  html/           index.html, qr.js/qr.wasm（静的配信 /html）
  Dockerfile      Render 用（既定 SQLite ビルド。TiDB/MySQL への切替はコメント参照）
python/           FastAPI 実装
  lambda_server.py  app（passkey/tenant ルータを /api 配下、/html 配信、/ は redirect）
  database.py       既定 SQLite / env で MySQL(TiDB) / DATABASE_URL 優先
  requirements.txt
```

## 共通の約束

- **ポートは環境変数 `POST`**（PORT ではない。既定 8080）。Render は `PORT` を注入するので、
  Python は `uvicorn ... --port $PORT`、C++ は Docker CMD で `POST=$PORT` に橋渡しする。
- **DB は既定 SQLite**（`test_db.db`、テーブルは起動時に自動作成）。
  永続化は **TiDB Cloud**：env `RDS_PROXY_HOST / USER_NAME / PASSWORD / DB_NAME`（Python は `DATABASE_URL` も可）。
- CORS 全許可。`/html/index.html` は 200（Render のヘルスチェックに使用）。`/` は `/html/index.html` へ redirect。

## ローカルで動かす

**Python（FastAPI）**
```sh
cd python
python -m venv .venv && . .venv/Scripts/activate      # mac/Linux は .venv/bin/activate
pip install -r requirements.txt
POST=8080 python lambda_server.py                      # http://localhost:8080/html/index.html
```

**C++（cpp-httplib）— Windows ローカル**

既定は **SQLite（依存ゼロ）**。`cpp/sqlpp11_cust.sln` を VS2022 で Release/x64 ビルド →
`cpp/` ディレクトリ（`html/` がある場所）から起動する：
```sh
cd cpp
POST=8080 ./x64/Release/sqlpp11_cust.exe        # http://localhost:8080/html/index.html
# DB はカレントに test_db.db を自動作成。DLL 不要。
```

**C++ を TiDB/MySQL で動かす（Windows・任意）**
1. `USE_MYSQL` を定義してビルド（`database.h` 冒頭の `#define USE_MYSQL 1` を有効化、
   もしくは MSBuild を `CL=-DUSE_MYSQL`（`/D` は Git Bash がパス変換して壊すのでダッシュ）で実行）。
2. 実行時に `libmysql.dll` を dlopen する。**`sh cpp/setup_local_dll.sh` を一度実行**すると、
   ローカルの MySQL 8.4 インストール（既定 `C:\prog\mysql-8.4.4-winx64`、`MYSQL_DIR` で変更可）から
   `libmysql.dll`＋openssl DLL を `x64/Release`・`x64/Debug`（exe の隣）にコピーする。
   → 以後 **PATH 設定不要**で exe が DLL を隣から読む。これらの DLL は `.gitignore` 済み（ローカル専用・非コミット）。
3. 接続情報を env で渡して起動：
   ```sh
   export PATH="/c/prog/mysql-8.4.4-winx64/lib:/c/prog/mysql-8.4.4-winx64/bin:$PATH"
   RDS_PROXY_HOST=... TIDB_PORT=4000 USER_NAME=... PASSWORD=... DB_NAME=test \
   POST=8080 ./x64/Release/sqlpp11_cust.exe
   ```

**C++ — Linux/Docker**
```sh
cd cpp && docker build -t parity-cpp . && docker run -e PORT=8080 -p 8080:8080 \
  -e RDS_PROXY_HOST=... -e TIDB_PORT=4000 -e USER_NAME=... -e PASSWORD=... -e DB_NAME=test parity-cpp
```

## Render へのデプロイ

1. この repo を push（`render.yaml` はリポ直下）。
2. Render → **New + → Blueprint** → この repo を選択 → 2サービス作成。
   - python … ネイティブ Python（`pip install` → `uvicorn`）
   - cpp … `cpp/Dockerfile` をビルド
3. `https://<service>.onrender.com/html/index.html` で確認。

無料枠は 15 分無アクセスでスピンダウン・起動約1分・**ディスク揮発**（永続化は TiDB へ）。

## TiDB Cloud（予定）

- Serverless（MySQL 互換・**TLS 必須**・port 4000）。接続情報は **env で注入し、リポジトリに秘密を置かない**（`secrets/` は gitignore 済み）。
- Python: `PyMySQL`（TLS 引数が要る）。C++: `USE_MYSQL` ビルド＋`libmysqlclient`（`cpp/Dockerfile` 末尾コメント）。

## ライセンス / 出典

元リポジトリ（passkey_server_cpp）に準じる。cpp-httplib(MIT), nlohmann/json(MIT), SQLite(public domain),
FastAPI/Uvicorn/SQLAlchemy(MIT 等), mbedtls 由来コード(Apache-2.0)。
